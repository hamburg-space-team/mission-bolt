#!/usr/bin/env bash
# Flash one or all boards over the debug station (tools/debug-station/).
# GDB transfers the ELF over the wire, so nothing is copied to the Pi and
# the binary on the board is exactly the one in out/.
#
#   scripts/flash.sh <btc|exp1|exp2|exp3> [Debug|Release]
#
# Deliberately one board per invocation: with a single probe, a batch mode
# would only invite flashing the wrong board
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

# Single probe: one gdb port; WHICH board gets flashed is decided by where
# the probe physically sits - put it on the board you name here
PORT=3401
declare -A MAP=(
    [btc]="btc"
    [exp1]="exp1-space-disco"
    [exp2]="exp2-bouncy-castle"
    [exp3]="exp3-floaty-boi"
)

# The station retries OpenOCD every 2 s while the probe is unplugged or
# mid-move; wait for the port instead of failing into that window
wait_for_station() {
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
    port="${PORT}"
    elf="${FSW}/out/${node}/${tgt}/${CFG}/${node}.elf"
    [ -f "${elf}" ] || { echo "error: ${elf} missing - build the ${CFG} context first"; return 1; }

    echo "== ${node}: ${elf#"${FSW}"/} -> ${HOST}:${port} (probe must sit on ${node}!)"
    wait_for_station || return 1

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
        -ex "monitor reset halt" \
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
