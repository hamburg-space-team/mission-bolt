import socket
import random
import time

PORT = 54321
SAMPLING_RATE = 0.04  # 40ms


class SmoothSensor:
    def __init__(self, min_val=-40, max_val=85, smoothness=0.05):
        self.min_val = min_val
        self.max_val = max_val
        self.smoothness = smoothness
        self.current_val = (min_val + max_val) / 2  # Start in the middle

    def next_sample(self):
        # 1. Generate a raw random target
        target = random.uniform(self.min_val, self.max_val)

        # 2. Linear Interpolation (LERP)
        # Move a small percentage (smoothness) toward the target
        self.current_val += (target - self.current_val) * self.smoothness

        # 3. Add a tiny bit of "jitter" (optional, for realism)
        jitter = random.uniform(-0.5, 0.5)

        return round(self.current_val + jitter, 2)


def start_sender():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        print("Booting up")
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(("0.0.0.0", PORT))
        s.listen(1)
        print(f"Server listening on port {PORT}")

        synth_sensor = SmoothSensor()

        conn, addr = s.accept()
        with conn:
            print(f"Connection established with {addr}")
            while True:
                value = synth_sensor.next_sample()

                message = f"{value:.4f}\n".encode("utf-8")
                conn.sendall(message)
                time.sleep(SAMPLING_RATE)


if __name__ == "__main__":
    start_sender()
