## Getting Started

### Prerequisites
- Python 3.8 or higher.
- No external library dependencies are required! The scripts utilize Python's standard library (`asyncio`, `struct`, `random`, `time`).

### Running the Simulation

To start the simulation, you should run the sender and receiver in separate terminal windows.

#### Step 1: Start the Telemetry Server (Sender)
The sender acts as the Moraba Ground Station. It binds to local port `5000` and starts broadcasting packaged, CRC-validated telemetry streams.

```bash
python sender.py
```

*You should see output similar to:*
```text
[*] Telemetry server listening on ('127.0.0.1', 5000)
[*] Starting BTC_ENV stream at 25.0 Hz
[*] Starting BTC_STATUS stream at 1.0 Hz
...
```

#### Step 2: Start the Telemetry Receiver (Client)
The receiver acts as your local ground station. It connects to the server and begins scanning the TCP byte-stream for the `0xB0 0x17` sync sequence.

```bash
python receiver.py
```

*By default, you will see output similar to:*
```text
[*] Connecting to telemetry server at 127.0.0.1:5000...
[*] Connected successfully.
[+] Valid Packet - Type: 0x10, Seq: 001, Length: 24, Tick: 00001, Total: 1
[+] Valid Packet - Type: 0x40, Seq: 001, Length: 40, Tick: 00001, Total: 2
...
```

**Live Dashboard Mode:**
To prevent your terminal from scrolling wildly, you can enable the live packet visualization dashboard by adding the `--dashboard` flag:

```bash
python receiver.py --dashboard
```

*This mode will clear the screen and draw a live bar chart of arriving packet volumes that updates every 1 second.*

---

## Configuration

You can easily adjust the simulation's behavior to test different operational scenarios.

### 1. Changing Cadences (Frequencies)
If you want to simulate reduced-rate bandwidth profiles (e.g., to test the **Minimum (50 kbit/s)** success criterion threshold), open `sender.py` and modify the `FREQUENCIES` dictionary at the top of the file:

```python
FREQUENCIES = {
    'BTC_ENV': 25.0,        # Frequency in Hz
    'BTC_STATUS': 1.0,
    # Scale down or comment out experiments to test minimum bandwidth margins:
    'EXP3_STACK_A': 10.0,   # Reduced from 50Hz nominal
    'EXP3_STACK_B': 10.0,
}
```

# TCPserver
