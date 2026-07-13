#!/usr/bin/env bash
# Recreate USB-serial /dev nodes inside the dev container.

set -euo pipefail

OWNER="${SERIAL_OWNER:-vscode}"   # extension host runs as vscode
found=0

for sys in /sys/class/tty/ttyUSB* /sys/class/tty/ttyACM*; do
    [ -e "$sys/dev" ] || continue
    name="$(basename "$sys")"
    node="/dev/$name"
    IFS=: read -r major minor < "$sys/dev"

    if [ ! -e "$node" ]; then
        sudo mknod "$node" c "$major" "$minor"
        echo "created $node (c $major:$minor)"
    fi
    sudo chown "$OWNER:$OWNER" "$node"
    sudo chmod 660 "$node"
    echo "ready: $(ls -la "$node")"
    found=1
done

[ "$found" = 1 ] || { echo "no ttyUSB*/ttyACM* in sysfs -- is the adapter attached via usbipd?"; exit 1; }
