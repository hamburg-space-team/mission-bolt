# Bolt Telemtry Tools

Live + post-flight telemetry review for **THHOR-BOLT (REXUS 37)**. A VS Code
extension for debugging and review, backed by a shared Rust codec that is the
single source of truth for the wire protocol.

## Build (manual, if not using the devcontainer's post-create)

```bash
cd telemetry-tools
cargo test                                  # all codec tests (bit-identical vectors)
cargo build                                 # the three binaries -> target/debug/ (HDF5 on by default)
# portable build without libhdf5:  cargo build --no-default-features
cd extension && npm install && npm run compile
```

The devcontainer's post-create builds this and installs the packaged extension
into the VS Code server automatically (it activates on startup). If you change
the extension, re-run `npm run compile`, repackage with `vsce package`, and
`--install-extension` the vsix (or reload the window).

## Run

**Live** (records as it goes):

```bash
bolt-serial-bridge --list-ports
bolt-serial-bridge --port tcp://bolt-station.local:5000 --raw captures/flight.raw
```

**Post-flight** (same overview, later):

```bash
bolt-postflight captures/flight.raw --manifest flight.manifest.json --hdf5 flight.h5
```

In the extension: **Connect Serial** enumerates the passed-through serial adapters
(via `--list-ports`, USB first, shown with VID/PID) and offers a pick; or **Open
.raw** for post-flight. A live session tees a `.raw`; on disconnect it is
auto-decoded to HDF5 + manifest, so the overview you saw live reopens identically offline.

## UI split

- **Sidebar** (control): Session (mode + connect/open), Stats (counts, CRC health,
  LO, experiments), Uplink (command buttons + arm), Export (HDF5 / manifest / CSV).
- **Windows** (data): the Overview editor (packets, timeline, CRC/faults) and, per
  experiment, correctly-scaled graphs + spectrum→RGB. `.raw` opens as a custom editor.
