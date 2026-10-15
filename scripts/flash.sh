#!/usr/bin/env bash
# Build one board's context and flash it over the debug station
# (tools/debug-station/). GDB transfers the ELF over the wire, so nothing
# is copied to the Pi and the binary on the board is exactly the one just
# built into out/.
#
#   scripts/flash.sh <btc|exp1|exp2|exp3> [Debug|Release]
#
# With more than one probe on the station, the right one is chosen for you:
# the station reads each target's chip UID and says which board it sits on,
# so naming a board picks its probe. Naming a board no probe reaches only
# ever proceeds after you say so - that is the case where a flash would
# land on the wrong hardware.
#
# Deliberately one board per invocation: a batch mode would only invite
# flashing the wrong board.
#
#   BOLT_PROBE=2   force a probe instead of resolving by board
#
# Station host: $BOLT_STATION (default bolt-station.local); ports follow
# the map in tools/debug-station/README.md.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FSW="$(cd "${HERE}/../flight-software" && pwd)"
HOST="${BOLT_STATION:-bolt-station.local}"
GDB="${GDB:-gdb-multiarch}"
NODE="${1:-}"
CFG="${2:-Debug}"

usage() { echo "usage: flash.sh <btc|exp1|exp2|exp3> [Debug|Release]"; exit 2; }
command -v "${GDB}" >/dev/null || { echo "error: ${GDB} not found"; exit 1; }

DEFAULT_PORT=3401
declare -A MAP=(
    [btc]="btc"
    [exp1]="exp1-space-disco"
    [exp2]="exp2-bouncy-castle"
    [exp3]="exp3-floaty-boi"
)

# Ask the station which board each probe is sitting on. Prints
# "<id> <gdb port> <board>" per probe; empty when the API is unreachable,
# which is not fatal - we fall back to the single-probe port.
probe_table() {
    curl -fsS --max-time 5 "http://${HOST}:8080/api/debugger" 2>/dev/null |
        python3 -c '
import json, sys
try:
    d = json.load(sys.stdin)
except Exception:
    sys.exit(0)
for p in d.get("probe", {}).get("probes", []):
    print(p["id"], p["gdb_port"], p.get("board") or "-")
' 2>/dev/null
}

# Resolve <board> -> gdb port, and refuse to be silently wrong about it.
resolve_port() {
    local node="$1" table id port board
    table="$(probe_table)"

    if [ -n "${BOLT_PROBE:-}" ]; then
        port="$(awk -v want="${BOLT_PROBE}" '$1 == want {print $2}' <<<"${table}")"
        [ -n "${port}" ] || { echo "error: no probe '${BOLT_PROBE}' on the station" >&2; return 1; }
        echo "   probe ${BOLT_PROBE} forced via BOLT_PROBE (gdb ${port})" >&2
        echo "${port}"
        return 0
    fi

    if [ -z "${table}" ]; then
        echo "   note: station API unreachable - using the default gdb port ${DEFAULT_PORT}" >&2
        echo "${DEFAULT_PORT}"
        return 0
    fi

    # a probe already sitting on the board we were asked for
    port="$(awk -v want="${node}" '$3 == want {print $2; exit}' <<<"${table}")"
    if [ -n "${port}" ]; then
        echo "${port}"
        return 0
    fi

    # none: whatever we flash now is NOT the board that was named
    echo "" >&2
    echo "   !! no probe is on ${node}. The station sees:" >&2
    while read -r id port board; do
        [ -n "${id}" ] || continue
        echo "      probe ${id} (gdb ${port}) -> ${board}" >&2
    done <<<"${table}"
    echo "" >&2
    echo "   Flashing now would write the ${node} image to whatever the probe" >&2
    echo "   below is actually touching." >&2
    id="$(awk 'NR == 1 {print $1}' <<<"${table}")"
    port="$(awk 'NR == 1 {print $2}' <<<"${table}")"
    board="$(awk 'NR == 1 {print $3}' <<<"${table}")"
    read -r -p "   Write ${node} via probe ${id} (on ${board})? [y/N] " reply
    case "${reply}" in
    y | Y | yes | YES) echo "${port}" ;;
    *) echo "   aborted" >&2; return 1 ;;
    esac
}

# The station retries OpenOCD every 2 s while the probe is unplugged or
# mid-move; wait for the port instead of failing into that window
wait_for_station() {
    local PORT="$1"
    for _ in $(seq 1 15); do
        if (exec 3<>"/dev/tcp/${HOST}/${PORT}") 2>/dev/null; then
            exec 3>&- 3<&-
            return 0
        fi
        echo "   station ${HOST}:${PORT} not ready (probe attached + board powered?) - retrying"
        sleep 2
    done
    echo "error: station ${HOST}:${PORT} unreachable after 30 s"
    return 1
}

flash_one() {
    local node="$1" tgt port elf
    tgt="${MAP[${node}]}"
    port="$(resolve_port "${node}")" || return 1
    elf="${FSW}/out/${node}/${tgt}/${CFG}/${node}.elf"

    # incremental cbuild is seconds; flashing a stale ELF costs a bench trip
    if command -v cbuild >/dev/null; then
        echo "== ${node}: cbuild ${node}.${CFG}+${tgt}"
        (cd "${FSW}" && cbuild bolt.csolution.yml --context "${node}.${CFG}+${tgt}" >/dev/null) \
            || { echo "== ${node}: build FAILED"; return 1; }
    else
        echo "   note: cbuild not found - flashing the existing ELF as-is"
    fi
    [ -f "${elf}" ] || { echo "error: ${elf} missing"; return 1; }

    echo "== ${node}: ${elf#"${FSW}"/} -> ${HOST}:${port} (probe must sit on ${node}!)"
    wait_for_station "${port}" || return 1

    # Fresh flash = fresh mission state: zero the .noinit magic so BootState
    # takes the cold path and the board comes up in TEST mode. A plain NRST
    # reset would do that too, but a probe without NRST wired resets via
    # SYSRESETREQ, which reads as warm and would restore the previous run's
    # mode - up to FLIGHT with a latched LO
    local noinit clear_state=()
    noinit="$(arm-none-eabi-objdump -h "${elf}" 2>/dev/null | awk '$2==".noinit" {print $4}')"
    if [ -n "${noinit}" ]; then
        clear_state=(-ex "set {unsigned int}0x${noinit} = 0")
    else
        echo "   note: no .noinit section found - mission state not cleared"
    fi

    "${GDB}" -batch -nx \
        -ex "target extended-remote ${HOST}:${port}" \
        -ex "monitor reset init" \
        -ex "load" \
        "${clear_state[@]}" \
        -ex "compare-sections" \
        -ex "monitor reset run" \
        "${elf}" || { echo "== ${node}: FAILED"; return 1; }
    echo "== ${node}: flashed, mission state cleared, running in TEST"
}

case "${NODE}" in
btc | exp1 | exp2 | exp3) flash_one "${NODE}" ;;
*) usage ;;
esac
