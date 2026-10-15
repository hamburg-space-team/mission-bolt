#!/usr/bin/env bash
# Hide the mouse pointer on the kiosk panel.
#
# The dashboard's CSS already sets `cursor: none`, but that only covers the
# page: the compositor draws a pointer over everything else, and while the
# page loads. The session is `cage` on Wayland (started from ~/.bash_profile
# on the autologin tty), so unclutter is no help - it is X11 only, and cage
# has no cursor flag. What wlroots does honour is XCURSOR_THEME, so point it
# at a theme whose images are transparent.
#
#   tools/debug-station/hide-cursor.sh          # run ON the Pi
#
# Takes effect on the next session start - the compositor reads the variable
# once, at startup, so a reboot is the way to apply it.
set -euo pipefail

THEME_DIR="${HOME}/.icons/blank/cursors"
# where the compositor is launched from; the variable has to be exported
# before it, not inside the kiosk script it runs
LAUNCHER="${HOME}/.bash_profile"

mkdir -p "${THEME_DIR}"

# A fully transparent Xcursor. The format is small enough to emit directly,
# which beats depending on xcursorgen being installed.
#
# One 1x1 image, but registered under every nominal size a compositor is
# likely to ask for: xcursor picks the closest available size and a theme
# that answers none of them falls back to the default - i.e. a visible
# pointer, which is exactly what we are trying to avoid.
python3 - "${THEME_DIR}/left_ptr" <<'PY'
import struct, sys

IMAGE = 0xFFFD0002
SIZES = [1, 12, 16, 24, 32, 48, 64]
# image chunk: 36-byte header + one ARGB pixel
CHUNK = 36 + 4

out = bytearray()
out += struct.pack("<4sIII", b"Xcur", 16, 0x0001_0000, len(SIZES))
first = 16 + 12 * len(SIZES)
for i, size in enumerate(SIZES):
    out += struct.pack("<III", IMAGE, size, first + i * CHUNK)
for size in SIZES:
    # header size, type, subtype (nominal size), version, w, h, xhot, yhot, delay
    out += struct.pack("<IIIIIIIII", 36, IMAGE, size, 1, 1, 1, 0, 0, 0)
    out += struct.pack("<I", 0x0000_0000)  # fully transparent

with open(sys.argv[1], "wb") as f:
    f.write(out)
PY

# Every name an app might ask for resolves to the same empty image; a missing
# name would fall back to the default theme and pop the pointer back
for name in default left_ptr pointer arrow hand1 hand2 text xterm crosshair \
    watch progress top_left_arrow grabbing; do
    [ "${name}" = "left_ptr" ] || ln -sf left_ptr "${THEME_DIR}/${name}"
done

cat >"${HOME}/.icons/blank/index.theme" <<'EOF'
[Icon Theme]
Name=blank
Comment=Transparent cursors for the BOLT kiosk panel
EOF

# Anything that ignores XCURSOR_THEME still looks up the theme named
# "default", so point that at ours too - belt and braces, since a single
# component falling back is enough to put the pointer back on screen
mkdir -p "${HOME}/.icons/default"
cat >"${HOME}/.icons/default/index.theme" <<'EOF'
[Icon Theme]
Name=default
Comment=Kiosk panel: inherit the transparent cursors
Inherits=blank
EOF

if [ ! -f "${LAUNCHER}" ]; then
    echo "error: ${LAUNCHER} not found - is this the kiosk Pi?" >&2
    exit 1
fi
if ! grep -q 'exec cage' "${LAUNCHER}"; then
    echo "error: no 'exec cage' line in ${LAUNCHER}; the session starts some" >&2
    echo "       other way now - export XCURSOR_THEME wherever that is" >&2
    exit 1
fi

# XCURSOR_SIZE too: xcursor picks the closest available size and does not
# scale, and the theme only carries a 1x1 image
if ! grep -q 'XCURSOR_THEME=blank' "${LAUNCHER}"; then
    sed -i 's|^\( *\)exec cage|\1export XCURSOR_THEME=blank XCURSOR_SIZE=1\n\1exec cage|' "${LAUNCHER}"
fi

echo "blank cursor theme installed in ${THEME_DIR}"
echo "XCURSOR_THEME exported in ${LAUNCHER}:"
grep -n 'XCURSOR\|exec cage' "${LAUNCHER}"
echo "reboot to apply - the compositor reads it once, at startup"
