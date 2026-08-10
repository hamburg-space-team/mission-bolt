# BOLT Debug Station

A Raspberry Pi with one ST-Link V3 turns flashing, debugging and
telemetry into network services - one cable carries all three, since the
downlink rides the probe's virtual COM port. The
devcontainer then needs no USB passthrough at all: `scripts/flash.sh`
flashes over TCP, the `* @ station` launch configs debug over TCP, and
the Bolt extension reads telemetry from `tcp://bolt-station.local:5000`.

## Hardware

- Raspberry Pi (any model with Ethernet; wired beats WLAN for GDB)
- 1x ST-Link, moved by hand onto whichever board is being worked on.
  All four boards are STM32L476, so one OpenOCD config serves them all;
  **which board you flash or debug is decided by where the probe sits**,
  the software cannot check it.
- No separate UART adapter: the downlink rides the ST-Link V3's **virtual
  COM port**, so one cable carries flash, debug and telemetry. udev names
  it `/dev/ttySTLINK`.

## Install (on the Pi)

```sh
git clone https://github.com/hamburg-space-team/mission-bolt
sudo mission-bolt/tools/debug-station/install.sh
```

This sets up OpenOCD and, as a bootstrap, ser2net for the telemetry
port; the bolt-station daemon below replaces the latter.

The udev rule names the ST-Link's VCP `/dev/ttySTLINK` by vendor id, so
nothing needs filling in for a single probe:

```sh
sudo udevadm control --reload && sudo udevadm trigger
ls -l /dev/ttySTLINK
```

## Port map (the contract every client uses)

| Service | Port |
|---|---|
| GDB (flash + debug, the one probe) | 3401 |
| OpenOCD telnet | 4401 |
| OpenOCD tcl (the station reads the probe through this) | 6601 |
| Downlink + uplink, ST-Link VCP at 230400 8N1 | 5000 |
| HTTP API + kiosk dashboard | 8080 |

`scripts/flash.sh`, the launch configs and the extension's debug-station
entry all assume this map and the hostname `bolt-station.local`
(override with `BOLT_STATION` for the script).

## Where is it? (DHCP moves the address)

The container has no mDNS resolver, so `bolt-station.local` is a plain
hosts entry that has to follow the lease:

```sh
scripts/set-station-ip.sh --discover   # sweep the subnet, find it, set it
scripts/set-station-ip.sh 10.0.0.5     # or set it by hand
scripts/set-station-ip.sh              # show the current one + verify
```

Discovery sweeps a /24 for the station's own HTTP API (~15 s), which is
the only party on the network that answers `/api/status`. Without a
mapping to derive the subnet from, give it one: `--discover 192.168.1`.
All three forms print what the station says about its own addressing,
e.g. `eth0: 134.28.54.90/27 (dhcp)`.

## Check it works

```sh
# from any machine on the network
scripts/flash.sh btc                       # flashes whatever the probe sits on!
arm-none-eabi-gdb -ex 'target extended-remote bolt-station.local:3401'
```

The services restart on failure and come back after reboot; `systemctl
status openocd-bolt bolt-station` on the Pi shows their state. Two GDB
clients are allowed (cortex-debug opens a second one for live watch).

## bolt-station daemon (this is what runs; ser2net is disabled)

`telemetry-tools/crates/bolt-station` owns the downlink UART - the
ST-Link V3 virtual COM port at 230400 - and re-serves the raw stream
**multi-client** on port 5000, so no client kicks another any more. The
same bytes feed an HTTP API on port 8080. Both rates are options
(`--port`, `--baud`, `--flight-baud`), so reading the RS-422 link
directly is still one flag away.

- `GET /api/status` - station IP and every interface with its CIDR and
  whether the address is DHCP or static, LO/SODS/SOE, which boards are
  sending (env/status freshness), current env values, mode per node, CRC
  quality all-time + 10-minute window, probe state
- `POST /api/window/reset` - restart the 10-minute window (packets,
  throughput and CRC fails are three views of it, so one reset clears all).
  The load percentage is measured against `--flight-baud` (230400 today,
  the same as the bench), so a faster bench cable can never hide how full
  the real downlink is
- `GET /api/tests` / `GET /api/tests/<id>` - full-system-test runs with
  an id each: totals count pass/fail only, the detail URL lists every
  step with name (from the wire contract), board result, raw value,
  receive wall-clock time and per-step duration. Runs open on the
  FULL_SYSTEM_TEST ack and close when every node reported or was skipped
- `GET /api/debugger` - ST-Link present, and WHICH board it sits on:
  the chip's 96-bit UID is read via OpenOCD and mapped through
  `/etc/bolt-station/uids.conf` (`<uid> <board>` per line; read a new
  board's UID from the same endpoint and append it once)
- `GET /api/debugger/noinit` - live .noinit read through the probe: tick,
  reboot count, boot reason, mode, LO latch. `?addr=0x...` overrides the
  scan. "No state" is an answer, not an error: it reports `valid: false`
  and whether the probe was reachable at all
- `GET /api/selftest/steps` - per-node step names from the wire contract
- `GET|POST /api/lo` - the REXUS LO line, faked on the ST-Link V3's bridge
  GPIO0 while the flight harness is not connected. `POST` drives it
  (`?level=high`, the default, or `?level=low`), `GET` only reports it -
  reading never starts driving the line. `--lo-gpio` moves it. The bridge
  is a USB interface of its own, so this works while OpenOCD holds the
  debug interface

  **GPIO0 never touches the LO line directly.** REXUS LO is 28 V pulled to
  GND, so the GPIO drives a low-side switch (N-FET or opto) that pulls the
  line down exactly as the RXSM does - hence **gpio high = LO asserted**.
  A pull-down on the gate is what makes an undriven pin safe: unplug the
  probe, reboot the Pi or never start the station at all, and the
  transistor stays off with LO released. Without it every cold start
  latches a liftoff, because the BTC's level fallback needs only 3 ticks.
  `--lo-idle low` (set in the unit file) covers the smaller case where the
  station drove the pin and then lost the probe; it re-parks the line
  within 2 s of the probe reappearing. Leave the flag off if a real RXSM
  harness is ever on that line - the station must not drive against it
- `POST /api/command/<name>` - encode an uplink command and send it,
  e.g. `full_system_test`

Build it on your machine, not on the Pi - cross-compiling takes a minute
where a native Pi 3 build takes many. The Pi runs 64-bit Debian, so the
target is `aarch64-unknown-linux-gnu`; the station never enumerates
serial ports (it opens `/dev/ttySTLINK` by path), so `serialport` drops
`libudev` and the binary needs nothing but libc:

```sh
sudo apt-get install gcc-aarch64-linux-gnu     # once
rustup target add aarch64-unknown-linux-gnu    # once

scripts/build-station.sh              # build only
scripts/build-station.sh --upload     # + scp to the Pi's /tmp, run by hand
scripts/build-station.sh --install    # + install as the service, restart
```

The unit file goes over once (the `--install` path expects it):

```sh
scp tools/debug-station/bolt-station.service bolt@bolt-station.local:/tmp/
ssh bolt@bolt-station.local '
  sudo cp /tmp/bolt-station.service /etc/systemd/system/
  sudo systemctl disable --now ser2net
  sudo systemctl daemon-reload && sudo systemctl enable --now bolt-station'
```

The test database is per-row (`run_id, node, test_id`), never one column
per test - adding or reordering firmware tests can only add rows, so it
survives every contract change.

`uids.conf` maps a chip to its board - that is all it needs, because the
station finds `.noinit` itself by scanning SRAM for the BootState magic
(the address moves with every build that changes `.bss`):

```text
# <chip uid>                <board>
004c00483631500620373741    btc
```

Put the probe on a board, read its UID from `GET /api/debugger`, append
the line. The file is re-read per request, so no restart. A duplicate uid
is reported under `config_warnings` rather than silently taking the first.

## Kiosk dashboard (480x320)

The daemon serves a dark ops dashboard at `/` on the same port as the
API, so the panel needs nothing but `http://localhost:8080` - **no
network at all**. Header: which boards are live and where the probe
sits, LO/SODS/SOE and the mission mode. Left: every self-test run with
its number, verdict and when it ran; tapping one opens the per-step
detail (contract name, board verdict, raw value, duration) with a back
button. Right: env values, CRC quality, `.noinit`. Footer: the station's
address and a FULL SYSTEM TEST button that arms once before firing
(the command is flagged dangerous in the contract).

The page itself never scrolls; only the run list and the step list do,
and no scrollbar is drawn. `scripts/build-station.sh --install` ships it
to `/usr/local/share/bolt-station/ui` together with the daemon.

The kiosk launcher (`/home/bolt/kiosk.sh` on the Pi) only ever needs:

```sh
chromium-browser --kiosk --app=http://localhost:8080 \
  --window-size=480,320 --noerrdialogs --disable-infobars
```

To work on the dashboard from the devcontainer, `npm run dev` in
`telemetry-tools/station-ui` proxies `/api` to the real station.

Planned on this daemon for the four-probe stage (API paths reserved
under `/api/queue`, deliberately unimplemented): upload a binary per
board, queue it, flash whichever board the probe reaches, run the full
system test, read the results - with pause/resume for manual work and
only the LAST binary per board kept on the Pi so a run can be repeated.
