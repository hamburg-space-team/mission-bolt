#!/usr/bin/env bash
# Point bolt-station.local at the debug station's current address. The Pi
# gets its IP via DHCP, so the address can move between sessions.
#
#   scripts/set-station-ip.sh 134.28.54.90    set, persist, verify
#   scripts/set-station-ip.sh                 show + verify current mapping
#
# Applies immediately to the running container (/etc/hosts, no rebuild) and
# persists the address in .devcontainer/devcontainer.json for the next one.
# Verifies over ssh (22) only - probing 5000 would kick an active telemetry
# session (ser2net kickolduser).
set -uo pipefail

HOST=bolt-station.local
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DCJ="${HERE}/../.devcontainer/devcontainer.json"
IP="${1:-}"

if [ -n "${IP}" ]; then
    if ! [[ "${IP}" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        echo "usage: set-station-ip.sh [<ipv4>]"
        exit 2
    fi
    # running container: replace any existing mapping. /etc/hosts is a bind
    # mount - write through it (cat >), a rename like sed -i fails with EBUSY
    tmp="$(mktemp)"
    grep -v "[[:space:]]${HOST}\b" /etc/hosts > "${tmp}"
    echo "${IP} ${HOST}" >> "${tmp}"
    sudo bash -c "cat '${tmp}' > /etc/hosts"
    rm -f "${tmp}"
    # next container: the docker-level mapping follows suit
    sed -i "s|--add-host=${HOST}:[^\"]*|--add-host=${HOST}:${IP}|" "${DCJ}"
    echo "${HOST} -> ${IP} (running container + devcontainer.json)"
fi

ip="$(getent hosts "${HOST}" | awk '{print $1; exit}')"
if [ -z "${ip}" ]; then
    echo "no mapping for ${HOST} - set one: set-station-ip.sh <ipv4>"
    exit 1
fi

if timeout 3 bash -c "exec 3<>/dev/tcp/${ip}/22" 2>/dev/null; then
    echo "station reachable: ${ip} (ssh up)"
else
    echo "WARN: ${HOST} = ${ip}, but ssh (22) is not reachable"
    echo "  wrong/stale IP, station down, or different network"
    exit 1
fi
