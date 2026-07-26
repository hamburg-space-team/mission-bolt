#!/usr/bin/env bash
# clang-tidy + clang-format gate for the flight software.
#
# clang-tidy replays the cbuild compile databases (flight-software/tmp/<n>/)
# through a sanitized copy, because they were written for arm-none-eabi-g++:
#   - the GCC-internal include dirs are dropped (clang ships its own
#     intrinsic headers; GCC's arm_acle.h does not parse under clang)
#   - -masm-syntax-unified is dropped (GCC-only flag)
#   - --target=arm-none-eabi is appended
#
# Scope: shared/ and */app/ C++ sources. lib/, CubeMX-generated code and the
# interfaces/ headers are excluded - interfaces has its own annotation-driven
# naming (lowercase `wire`/`packet`) that the flight naming rules would flag.
#
# Usage: lint-flight.sh [--fix]     JOBS=<n> to cap parallelism
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FSW="$(cd "${HERE}/../flight-software" && pwd)"
JOBS="${JOBS:-$(nproc)}"
FIX=""
[ "${1:-}" = "--fix" ] && FIX="--fix"

for tool in clang-tidy clang-format jq; do
    command -v "${tool}" >/dev/null || { echo "error: ${tool} not found"; exit 1; }
done

cd "${FSW}"

# Compile databases come from cbuild; produce them if missing
if ! ls tmp/*/compile_commands.json >/dev/null 2>&1; then
    echo "no compile databases - running cbuild once"
    cbuild bolt.csolution.yml \
        --context btc.Debug+btc --context exp1.Debug+exp1-space-disco \
        --context exp2.Debug+exp2-bouncy-castle --context exp3.Debug+exp3-floaty-boi \
        >/dev/null || { echo "error: cbuild failed"; exit 1; }
fi

SAN="$(mktemp -d)"
trap 'rm -rf "${SAN}"' EXIT

# Merge the per-context DBs into one clang-digestible DB, one entry per file
# (shared/ files are compiled in all four contexts; any one context will do)
jq -s '
  [ add[]
    | select(.file | test("/(shared|app)/.*\\.cpp$"))
    | select(.file | test("/lib/") | not)
    | .command |= (
        gsub(" -masm-syntax-unified"; "")
        | gsub(" -isystem [^ ]*lib/gcc[^ ]*"; "")
        | . + " --target=arm-none-eabi -Wno-unknown-warning-option"
      )
  ] | unique_by(.file)
' tmp/*/compile_commands.json > "${SAN}/compile_commands.json"

mapfile -t FILES < <(jq -r '.[].file' "${SAN}/compile_commands.json" | sort)
[ "${#FILES[@]}" -gt 0 ] || { echo "error: no files matched the lint scope"; exit 1; }

RC=0

echo "== clang-tidy: ${#FILES[@]} files, ${JOBS} jobs"
if ! printf '%s\n' "${FILES[@]}" | xargs -P "${JOBS}" -n 1 \
    clang-tidy -p "${SAN}" --quiet --warnings-as-errors='*' \
    --header-filter='/(shared|app)/' ${FIX}; then
    RC=1
fi

# Format check covers headers too (tidy only sees them through TUs)
echo "== clang-format check"
mapfile -t FMT_FILES < <(git -C "${FSW}/.." ls-files -- \
    'flight-software/shared/**/*.hpp' 'flight-software/shared/**/*.cpp' \
    'flight-software/*/app/**/*.hpp' 'flight-software/*/app/**/*.cpp' \
    | sed "s|^|${FSW}/../|")
# an empty list means git could not see the repo (e.g. ownership) - fail loudly
[ "${#FMT_FILES[@]}" -gt 0 ] || { echo "error: git ls-files returned no sources"; exit 1; }
if [ -n "${FIX}" ]; then
    clang-format -i "${FMT_FILES[@]}"
elif ! clang-format --dry-run --Werror "${FMT_FILES[@]}"; then
    RC=1
fi

if [ "${RC}" -eq 0 ]; then
    echo "lint: clean"
else
    echo "lint: FINDINGS (re-run with --fix to auto-apply where possible)"
fi
exit "${RC}"
