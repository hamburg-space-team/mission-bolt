import asyncio
import socket
from contextlib import asynccontextmanager
from fastapi import FastAPI, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import HTMLResponse, StreamingResponse


# 1. Define the Lifespan (The modern way to handle startup/shutdown)
@asynccontextmanager
async def lifespan(app: FastAPI):
    # --- Startup Logic ---
    # Create a task for our socket listener
    task = asyncio.create_task(socket_reader())
    print("Background socket reader started.")

    yield  # The app runs here

    # --- Shutdown Logic ---
    task.cancel()
    try:
        await task
    except asyncio.CancelledError:
        print("Background socket reader cleaned up.")


app = FastAPI(lifespan=lifespan)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173", "http://127.0.0.1:5173"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


# 2. Shared State Manager
class SSEManager:
    def __init__(self):
        self.active_connections: list[asyncio.Queue[str]] = []

    async def connect(self):
        queue: asyncio.Queue[str] = asyncio.Queue()
        self.active_connections.append(queue)
        return queue

    def disconnect(self, queue: asyncio.Queue[str]):
        if queue in self.active_connections:
            self.active_connections.remove(queue)

    async def broadcast(self, message: str):
        for queue in list(self.active_connections):
            await queue.put(message)


manager = SSEManager()


# 3. The Socket Worker
async def socket_reader():
    host, port = "127.0.0.1", 54321
    loop = asyncio.get_running_loop()

    while True:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setblocking(False)

        print(f"Attempting to connect to sensor server at {host}:{port}...")

        try:
            # Connect as a client
            await loop.sock_connect(sock, (host, port))
            print("Successfully connected to sensor!")

            while True:
                # Read 4 bytes for the float
                data = await loop.sock_recv(sock, 1024)

                if not data:
                    print("Sensor server closed the connection.")
                    break

                # if len(data) == 4:
                # value = struct.unpack("f", data)[0]
                await manager.broadcast(data.decode().strip())

        except (ConnectionRefusedError, OSError) as e:
            print(f"Connection failed: {e}. Retrying in 2 seconds...")
            await asyncio.sleep(2)
        except Exception as e:
            print(f"Unexpected error: {e}")
            await asyncio.sleep(2)
        finally:
            sock.close()


# 4. Standard Routes
@app.get("/")
async def get():
    return HTMLResponse(html_content)


@app.get("/events")
async def events(request: Request):
    queue = await manager.connect()

    async def event_generator():
        try:
            while True:
                if await request.is_disconnected():
                    break

                try:
                    message = await asyncio.wait_for(queue.get(), timeout=1.0)
                except asyncio.TimeoutError:
                    continue

                yield f"data: {message}\n\n"
        finally:
            manager.disconnect(queue)

    return StreamingResponse(
        event_generator(),
        media_type="text/event-stream",
        headers={
            "Cache-Control": "no-cache",
            "Connection": "keep-alive",
            "X-Accel-Buffering": "no",
        },
    )


# HTML for testing
html_content = """
<!DOCTYPE html>
<html>
    <body>
        <h1>Sensor Value: <span id="val">--</span></h1>
        <script>
            const events = new EventSource("/events");
            events.onmessage = (event) => {
                document.getElementById('val').innerText = event.data;
            };
            events.onerror = () => {
                console.error('SSE connection error');
            };
        </script>
    </body>
</html>
"""
