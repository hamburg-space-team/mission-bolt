import socket

PORT = 54321


def start_receiver():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            s.connect(("0.0.0.0", PORT))
            print("Connected! Receiving data")

            while True:
                data = s.recv(1024)
                if not data:
                    break
                print(data.decode("utf-8").strip())

        except ConnectionRefusedError:
            print("Error: Could not connect to the server. Is it running?")
        except KeyboardInterrupt:
            print("Sender stopped.")


if __name__ == "__main__":
    start_receiver()
