# BOLT Debug Station

A Raspberry Pi with one ST-Link and the RXSM simulator's UART attached
turns flashing, debugging and telemetry into network services. The
devcontainer then needs no USB passthrough at all: `scripts/flash.sh`
flashes over TCP, the `* @ station` launch configs debug over TCP, and
the Bolt extension reads telemetry from `tcp://bolt-station.local:5000`.

## Hardware

- Raspberry Pi (any model with Ethernet; wired beats WLAN for GDB)
- 1x ST-Link, moved by hand onto whichever board is being worked on.
  All four boards are STM32L476, so one OpenOCD config serves them all;
  **which board you flash or debug is decided by where the probe sits**,
  the software cannot check it.
- 1x USB-UART adapter on the RXSM simulator's RS-422 telemetry output
  (the sim itself stays wired exactly as before; only the PC side moves
  to the network)

## Install (on the Pi)

```sh
git clone https://github.com/hamburg-space-team/mission-bolt
sudo mission-bolt/tools/debug-station/install.sh
```

Then give the RXSM UART adapter its stable name and restart:

1. `/etc/udev/rules.d/99-bolt-station.rules` - the adapter's serial, so
   it always appears as `/dev/ttyRXSM` no matter the USB port.
   Find it with: `udevadm info -a -n /dev/ttyUSB0 | grep '{serial}'`

```sh
sudo udevadm control --reload && sudo udevadm trigger
sudo systemctl restart openocd-bolt ser2net
```

## Port map (the contract every client uses)

| Service | Port |
|---|---|
| GDB (flash + debug, the one probe) | 3401 |
| OpenOCD telnet | 4401 |
| OpenOCD tcl | 6601 |
| RXSM telemetry (RS-422 downlink + uplink, 38400 8N1) | 5000 |

`scripts/flash.sh`, the launch configs and the extension's debug-station
entry all assume this map and the hostname `bolt-station.local`
(override with `BOLT_STATION` for the script).

## Check it works

```sh
# from any machine on the network
scripts/flash.sh btc                       # flashes whatever the probe sits on!
arm-none-eabi-gdb -ex 'target extended-remote bolt-station.local:3401'
nc bolt-station.local 5000 | head -c 64 | xxd   # raw downlink bytes
```

The services restart on failure and come back after reboot; `systemctl
status openocd-bolt ser2net` on the Pi shows their state. One GDB client
at a time; the telemetry socket kicks the previous reader when a new one
connects, so a forgotten `nc` never blocks the extension.

## Later: one probe per board

When four probes arrive, pin each to its board by serial (`adapter
serial` in per-board configs on distinct ports 3401-3404) and turn the
service into a template - the port map in the clients is already laid
out so btc..exp3 map to 3401..3404.
