#!/usr/bin/env bash
# Cross-build bolt-station for the debug station (Raspberry Pi, aarch64),
# build the kiosk dashboard, and optionally upload both.
#
#   scripts/build-station.sh                 build only
#   scripts/build-station.sh --upload        build, scp to the Pi's /tmp
#   scripts/build-station.sh --install       build, upload, install + restart
#
# The daemon serves the dashboard itself: one origin, no second server.
#
# Host: $BOLT_STATION (default bolt-station.local), user $BOLT_STATION_USER
# (default bolt). Needs, once:
#   sudo apt-get install gcc-aarch64-linux-gnu
#   rustup target add aarch64-unknown-linux-gnu
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TT="$(cd "${HERE}/../telemetry-tools" && pwd)"
HOST="${BOLT_STATION:-bolt-station.local}"
USER_NAME="${BOLT_STATION_USER:-bolt}"
TARGET=aarch64-unknown-linux-gnu
BIN="${TT}/target/${TARGET}/release/bolt-station"
UI="${TT}/station-ui"
UI_DEST=/usr/local/share/bolt-station/ui
MODE="${1:-}"

command -v aarch64-linux-gnu-gcc >/dev/null ||
    { echo "error: aarch64-linux-gnu-gcc missing (sudo apt-get install gcc-aarch64-linux-gnu)"; exit 1; }
rustup target list --installed | grep -qx "${TARGET}" ||
    { echo "error: rust target missing (rustup target add ${TARGET})"; exit 1; }

echo "== cross build ${TARGET}"
(cd "${TT}" && cargo build --release -p bolt-station --target "${TARGET}") || exit 1

# an aarch64 ELF with no libudev, or it will not run over there
aarch64-linux-gnu-readelf -h "${BIN}" | grep -q AArch64 ||
    { echo "error: built binary is not AArch64"; exit 1; }
if aarch64-linux-gnu-readelf -d "${BIN}" | grep -q libudev; then
    echo "error: binary needs libudev - the Pi image does not carry the arm64 dev libs"
    exit 1
fi
echo "built: ${BIN}"

echo "== kiosk dashboard"
(cd "${UI}" && { [ -d node_modules ] || npm install --silent; } && npm run --silent build) || exit 1
[ -f "${UI}/dist/index.html" ] || { echo "error: dashboard build produced no index.html"; exit 1; }
echo "built: ${UI}/dist"

# The container has no key of its own: ssh rides on the forwarded agent.
# Without it (docker exec, or a local sudo) ssh would only say
# "Permission denied (publickey)", so name what is actually missing
if [ -n "${MODE}" ] && ! ssh-add -l >/dev/null 2>&1; then
    echo "error: no ssh agent keys available (SSH_AUTH_SOCK=${SSH_AUTH_SOCK:-unset})"
    echo "  this container holds no key of its own - it uses the forwarded agent."
    echo "  open a terminal from the VS Code window, and do not run this with sudo."
    exit 1
fi

case "${MODE}" in
"") exit 0 ;;
--upload)
    scp "${BIN}" "${USER_NAME}@${HOST}:/tmp/bolt-station" || exit 1
    scp -r "${UI}/dist" "${USER_NAME}@${HOST}:/tmp/bolt-ui" || exit 1
    echo "uploaded to ${HOST}:/tmp/{bolt-station,bolt-ui} - --install makes them live"
    ;;
--install)
    scp "${BIN}" "${USER_NAME}@${HOST}:/tmp/bolt-station" || exit 1
    scp -r "${UI}/dist" "${USER_NAME}@${HOST}:/tmp/bolt-ui" || exit 1
    # ser2net and the station both want port 5000; only one may own it
    ssh "${USER_NAME}@${HOST}" "
        sudo systemctl disable --now ser2net 2>/dev/null
        sudo install /tmp/bolt-station /usr/local/bin/bolt-station &&
        sudo rm -rf ${UI_DEST} && sudo mkdir -p ${UI_DEST} &&
        sudo cp -r /tmp/bolt-ui/. ${UI_DEST}/ &&
        rm -rf /tmp/bolt-ui &&
        sudo systemctl restart bolt-station 2>/dev/null ||
            echo 'note: bolt-station.service not installed yet - see tools/debug-station/README.md'
    " || exit 1
    echo "installed on ${HOST} (daemon + dashboard)"
    ;;
*)
    echo "usage: build-station.sh [--upload|--install]"
    exit 2
    ;;
esac
