# Remote Flash, Debug and Telemetry

All board access goes through the debug station: a Raspberry Pi with the
four ST-Links and the RXSM simulator's UART attached
([`tools/debug-station/`](../../tools/debug-station/README.md) provisions
it). The devcontainer needs no USB passthrough; everything below works
over the network.

## Flash

```sh
scripts/flash.sh btc              # one board, Debug
scripts/flash.sh exp2 Release     # one board, Release image
BOLT_STATION=10.0.0.7 scripts/flash.sh exp2   # non-default host
```

One board per invocation, deliberately: with a single probe, a batch
mode would only invite flashing the wrong board.

GDB transfers the ELF over the connection, so the flashed binary is
exactly the local `out/` build; `compare-sections` in the script
verifies it. The script also zeroes the `.noinit` mission state, so a
freshly flashed board always boots into TEST mode - never into a
warm-restored FLIGHT from the previous run.

## Debug

Run and Debug view -> `btc @ station` (or exp1/2/3). These configs build
first, connect to the station's GDB port, flash, and stop at
`app_main` - same behaviour as the local `Debug btc` configs, which
remain for a directly attached probe.

## Telemetry

The RXSM downlink (and the uplink back) is a TCP socket on port 5000.
In the Bolt extension's connect picker choose **Debug station**, or
anywhere the bridge runs:

```sh
bolt-serial-bridge --port tcp://bolt-station.local:5000
```

Uplink commands work over the same connection. The RXSM simulator
itself stays wired to the station by UART as always; only the PC side
is a network client now.

## Port map

One probe, one GDB port: **3401**. Which board you reach is decided by
where the probe physically sits - `flash.sh btc` flashes whatever the
probe is clipped to, so check the probe before you flash. Telemetry:
5000. Full map and the station-side details:
[`tools/debug-station/README.md`](../../tools/debug-station/README.md).

The station retries OpenOCD every 2 s while the probe is unplugged or a
board is unpowered; `flash.sh` waits up to 30 s for the port instead of
failing into that window. One GDB client at a time; a second connect to
the telemetry socket kicks the first (deliberate, so a forgotten reader
never blocks the bench).
