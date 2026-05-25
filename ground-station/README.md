# Ground Station

Receive, decode, and display the THHOR-BOLT downlink. Runs on a
laptop at the launch site (or anywhere with TCP access to the
upstream packet handler).

For the architecture overview, see CDR §4.7 and
[../architecture.md](../architecture.md).

## What's Here

```
ground-station/
├── pyproject.toml         Python project (FastAPI sensor-api)
├── uv.lock                Pinned Python deps
├── sensor-api/            Bridge from packet socket to SSE
│   ├── main.py            FastAPI variant
│   ├── main.go            Go (Gin) variant
│   └── go.mod
├── sensor-dashboard/      React + Vite frontend
│   ├── package.json
│   └── src/
├── synth_data.py          Synthetic packet generator
└── socket_client.py       Minimal SSE smoke-test client
```

## Pieces

1. **Packet receiver** — decodes the THHOR-BOLT framing
   ([ICD-006](../docs/interfaces/ICD-006-downlink-packet-format.md))
   from whatever upstream gives us bytes, validates CRC, drops
   corrupted frames. Currently this logic lives inside the
   sensor-api; TODO is to split it once we have more than one
   consumer.
2. **Sensor-api** — FastAPI service that connects to a TCP socket
   (the packet receiver) and fans out incoming packets as
   Server-Sent Events. Go variant available for the same role.
3. **Sensor-dashboard** — React + Vite UI that subscribes to the
   SSE stream and renders live telemetry.

The three pieces are decoupled on purpose. If the backend dies,
the receiver keeps storing data. Raw incoming traffic should also
be captured with Wireshark to a `.pcap` as a third safety net.

## Run

See [docs/guides/running-ground-station.md](../docs/guides/running-ground-station.md)
for the full sequence with synthetic data.

Quick:

```bash
cd ground-station
uv sync && source .venv/bin/activate

# Terminal 1: synthetic data
python synth_data.py

# Terminal 2: sensor-api
cd sensor-api && uvicorn main:app --reload

# Terminal 3: dashboard
cd sensor-dashboard && bun install && bun run dev
```

Open <http://localhost:5173>.

## Toolchain

- Python 3.12+ via `uv`
- Node 20+ via `bun`
- Go 1.25+ (only for the Go sensor-api variant)

## Standards

- [Python standard](../docs/standards/coding/python.md)
- [JavaScript / React standard](../docs/standards/coding/javascript.md)

## Related Repositories

- [bolt-flight-analytics](https://github.com/hamburg-space-team/bolt-flight-analytics) — post-flight analysis
- [bolt-thhor-hil](https://github.com/hamburg-space-team/bolt-thhor-hil) — HIL platform that produces realistic packet streams during development
