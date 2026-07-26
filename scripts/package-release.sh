#!/usr/bin/env bash
# Assemble the release payload into dist/: the four flight images plus the
# wire contract they speak. Release images (.hex/.bin/.elf) are what gets
# flashed; the Debug ELFs ride along because FAULT line numbers are only
# decodable against the exact build's symbols. SHA256SUMS ties a flashed
# board to a commit.
#
# Expects Debug AND Release contexts built (cbuild, both build-types).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "${HERE}/.." && pwd)"
FSW="${REPO}/flight-software"
DIST="${REPO}/dist"

command -v arm-none-eabi-objcopy >/dev/null || { echo "error: arm-none-eabi-objcopy not found"; exit 1; }

rm -rf "${DIST}"
mkdir -p "${DIST}"

PAIRS=(btc:btc exp1:exp1-space-disco exp2:exp2-bouncy-castle exp3:exp3-floaty-boi)
for p in "${PAIRS[@]}"; do
    proj="${p%%:*}"
    tgt="${p#*:}"
    rel="${FSW}/out/${proj}/${tgt}/Release/${proj}.elf"
    dbg="${FSW}/out/${proj}/${tgt}/Debug/${proj}.elf"
    [ -f "${rel}" ] || { echo "error: ${rel} missing - build the Release contexts first"; exit 1; }
    [ -f "${dbg}" ] || { echo "error: ${dbg} missing - build the Debug contexts first"; exit 1; }
    cp "${rel}" "${DIST}/${proj}-release.elf"
    cp "${dbg}" "${DIST}/${proj}-debug.elf"
    arm-none-eabi-objcopy -O ihex "${rel}" "${DIST}/${proj}.hex"
    arm-none-eabi-objcopy -O binary "${rel}" "${DIST}/${proj}.bin"
done

# The wire contract of exactly this build: without it a capture from a
# flashed board cannot be decoded with certainty later
cp "${REPO}/interfaces/tools/generated/schema.json" \
   "${REPO}/docs/interfaces/ICD-007-packet-payloads.md" \
   "${REPO}/interfaces/tools/generated/icd-007.tex" \
   "${DIST}/"

(cd "${DIST}" && sha256sum -- * > SHA256SUMS)
echo "release payload:"
ls -l "${DIST}"
