# Running the Ground Station

Receive-and-display side. Decodes the downlink stream from the
REXUS PCM chain (or a synthetic source) and shows live telemetry.

Architecture is a packet receiver -> SSE bridge -> React dashboard.
See [`ground-station/README.md`](../../ground-station/README.md)
and CDR section 4.7 for the full picture.

## Prereqs

- Python 3.12+
- `uv` (or `pip` as fallback)
- Node 20+
- `bun` (or `npm` as fallback)
- Optional, for the Go variant: Go 1.25+

## Layout

```
ground-station/
+-- pyproject.toml          Python project for FastAPI sensor-api
+-- sensor-api/             Sensor API: Python (FastAPI) and Go variants
+-- sensor-dashboard/       React + Vite frontend
+-- synth_data.py           Synthetic packet generator
+-- socket_client.py        Minimal smoke-test client
```

## First-Time Setup

```bash
cd ground-station
uv sync                     # creates .venv with Python deps
source .venv/bin/activate

cd sensor-dashboard
bun install
```

For the Go sensor-api:

```bash
cd ground-station/sensor-api
go mod download
```

## With Synthetic Data

Three terminals.

**1) Packet source:**

```bash
cd ground-station
source .venv/bin/activate
python synth_data.py
```

Listens on `127.0.0.1:54321`.

**2) Sensor-api:**

```bash
cd ground-station/sensor-api
uvicorn main:app --reload --host 127.0.0.1 --port 8000
```

`fastapi dev main.py` works too.

**3) Dashboard:**

```bash
cd ground-station/sensor-dashboard
bun run dev
```

Open <http://localhost:5173>. Live values flow.

## With Flight Hardware

TODO: document the actual REXUS receive-PC wiring at IPR. For now,
point the packet receiver at whichever TCP socket the upstream
handler binds to (usually a USB serial adapter wrapped in `socat`).

The decoder validates CRC-16/CCITT
([ICD-006](../interfaces/ICD-006-downlink-packet-format.md)) and
drops bad frames. Decode errors show up in the sensor-api log.

## Smoke Test

Without the full dashboard:

```bash
cd ground-station
source .venv/bin/activate
python socket_client.py
```

Connects to the sensor-api's SSE endpoint and prints events. Good
for confirming the data path before debugging React.

## Capturing Raw Traffic

Use Wireshark on the network port that feeds the receiver. Saves
to `.pcap`. Useful for post-flight reconciliation when SD recovery
is incomplete.

## Common Issues

- **Dashboard "SSE connection error"** -- sensor-api not running, or
  CORS blocking the origin. Check `allow_origins` in
  [`sensor-api/main.py`](../../ground-station/sensor-api/main.py).
- **`uv: command not found`** -- install per
  <https://docs.astral.sh/uv/getting-started/installation/>.
- **Port 54321 in use** -- `lsof -i :54321`, kill or pick a
  different port via the synthetic source.
- **Synthetic source produces nothing** -- local firewall blocking
  loopback.

## Related

- [`ground-station/README.md`](../../ground-station/README.md)
- [ICD-006 Downlink Packet Format](../interfaces/ICD-006-downlink-packet-format.md)
- [ICD-007 Packet Payloads](../interfaces/ICD-007-packet-payloads.md)
