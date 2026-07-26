#!/usr/bin/env bash
# Provision a Raspberry Pi as the BOLT debug station. Run ON the Pi:
#   sudo tools/debug-station/install.sh
# Idempotent; an existing udev rule is kept.
set -euo pipefail

[ "$(id -u)" = 0 ] || { echo "run with sudo"; exit 1; }
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

apt-get update
apt-get install -y --no-install-recommends openocd ser2net

mkdir -p /etc/bolt-station
cp "${HERE}/station.cfg" /etc/bolt-station/
cp "${HERE}/ser2net.yaml" /etc/ser2net.yaml
[ -f /etc/udev/rules.d/99-bolt-station.rules ] || cp "${HERE}/99-bolt-station.rules" /etc/udev/rules.d/
cp "${HERE}/openocd-bolt.service" /etc/systemd/system/

udevadm control --reload
systemctl daemon-reload
systemctl enable --now ser2net openocd-bolt

echo
echo "now name the RXSM UART adapter and restart:"
echo "  /etc/udev/rules.d/99-bolt-station.rules    (adapter serial -> /dev/ttyRXSM)"
echo "  sudo udevadm control --reload && sudo udevadm trigger"
echo "  sudo systemctl restart openocd-bolt ser2net"
