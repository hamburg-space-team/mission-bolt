#!/usr/bin/env bash
# Point bolt-station.local at the debug station's current address. The Pi
# gets its IP via DHCP, so the address can move between sessions.
#
#   scripts/set-station-ip.sh --discover      find it on the network, then set
#   scripts/set-station-ip.sh 134.28.54.90    set, persist, verify
#   scripts/set-station-ip.sh                 show + verify current mapping
#
# Applies immediately to the running container (/etc/hosts, no rebuild) and
# persists the address in .devcontainer/devcontainer.json for the next one.
# Verifies over ssh (22) only - probing 5000 would kick an active telemetry
# session.
#
# --discover sweeps a /24 for the station's own HTTP API - mDNS is not
# usable here, which is why this hosts mapping exists at all. The subnet
# defaults to the current mapping's; override it: --discover 192.168.1
set -uo pipefail

HOST=bolt-station.local
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DCJ="${HERE}/../.devcontainer/devcontainer.json"
API_PORT=8080
IP="${1:-}"

current_ip() { getent hosts "${HOST}" | awk '{print $1; exit}'; }

# Is <ip> a bolt station? Its API says so in one request.
probe_station() {
    timeout 2 curl -s --max-time 2 "http://$1:${API_PORT}/api/status" 2>/dev/null |
        grep -q '"station"'
}

discover() {
    local prefix="${1:-}"
    if [ -z "${prefix}" ]; then
        local cur
        cur="$(current_ip)"
        [ -n "${cur}" ] || { echo "no current mapping to derive a subnet from - pass one: --discover 192.168.1"; exit 2; }
        prefix="${cur%.*}"
    fi
    echo "scanning ${prefix}.0/24 for a bolt station on :${API_PORT}" >&2
    # 254 probes, 32 at a time; the first hit wins
    local found
    found="$(seq 1 254 | xargs -P 32 -I{} bash -c "
        timeout 2 curl -s --max-time 2 http://${prefix}.{}:${API_PORT}/api/status 2>/dev/null |
            grep -q '\"station\"' && echo ${prefix}.{}" 2>/dev/null | head -1)"
    [ -n "${found}" ] || { echo "no station answered in ${prefix}.0/24" >&2; exit 1; }
    echo "${found}"
}

if [ "${IP}" = "--discover" ]; then
    IP="$(discover "${2:-}")" || exit 1
    echo "found station at ${IP}"
fi

if [ -n "${IP}" ]; then
    if ! [[ "${IP}" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        echo "usage: set-station-ip.sh [--discover [<a.b.c>] | <ipv4>]"
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

ip="$(current_ip)"
if [ -z "${ip}" ]; then
    echo "no mapping for ${HOST} - find it: set-station-ip.sh --discover"
    exit 1
fi

if timeout 3 bash -c "exec 3<>/dev/tcp/${ip}/22" 2>/dev/null; then
    echo "station reachable: ${ip} (ssh up)"
else
    echo "WARN: ${HOST} = ${ip}, but ssh (22) is not reachable"
    echo "  wrong/stale IP, station down, or different network"
    exit 1
fi

# what the station says about its own addressing
timeout 3 curl -s --max-time 3 "http://${ip}:${API_PORT}/api/status" 2>/dev/null |
    python3 -c '
import json, sys
try:
    s = json.load(sys.stdin)["station"]
except Exception:
    sys.exit(0)
print("  hostname: " + str(s.get("hostname")))
for i in s.get("interfaces", []):
    print("  {}: {} ({})".format(i["interface"], i["cidr"], i["source"]))
'
exit 0
