#!/usr/bin/env bash
# One-stop verification for mission-bolt. Run it after any change - it is the
# same gate for everyone (and for CI): flight builds, host unit tests,
# generated-artifact drift, ground tooling, lint.
#
#   scripts/verify.sh                 full gate
#   scripts/verify.sh --no-lint      skip clang-tidy/clang-format (slowest part)
#   scripts/verify.sh --no-build     skip the flight cbuild
#   scripts/verify.sh --no-rust      skip cargo checks
#   scripts/verify.sh --no-ext       skip the extension typecheck
#
# Sections report PASS/FAIL/SKIP; the script exits non-zero if anything FAILs.
# SKIPs (missing optional tooling) never fail the gate, but are listed.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "${HERE}/.." && pwd)"
FSW="${REPO}/flight-software"
TT="${REPO}/telemetry-tools"

DO_LINT=1 DO_BUILD=1 DO_RUST=1 DO_EXT=1
for arg in "$@"; do
    case "${arg}" in
    --no-lint) DO_LINT=0 ;;
    --no-build) DO_BUILD=0 ;;
    --no-rust) DO_RUST=0 ;;
    --no-ext) DO_EXT=0 ;;
    *) echo "unknown option: ${arg}"; exit 2 ;;
    esac
done

declare -A RESULT
ORDER=()
FAILED=0

section() { # section <name> <cmd...>
    local name="$1"; shift
    ORDER+=("${name}")
    echo
    echo "===== ${name} ====="
    if "$@"; then
        RESULT[${name}]=PASS
    else
        RESULT[${name}]=FAIL
        FAILED=1
    fi
}

skip() { # skip <name> <reason>
    ORDER+=("$1")
    RESULT[$1]="SKIP ($2)"
    echo
    echo "===== $1 ===== skipped: $2"
}

# --- flight build: all four Debug contexts --------------------------------
flight_build() {
    (cd "${FSW}" && cbuild bolt.csolution.yml \
        --context btc.Debug+btc --context exp1.Debug+exp1-space-disco \
        --context exp2.Debug+exp2-bouncy-castle --context exp3.Debug+exp3-floaty-boi)
}

# --- host unit tests (Catch2 via ctest) -----------------------------------
host_tests() {
    local bld="${FSW}/build/tests"
    cmake -S "${FSW}" -B "${bld}" >/dev/null &&
        cmake --build "${bld}" >/dev/null &&
        ctest --test-dir "${bld}" --output-on-failure
}

# --- generated artifacts in sync with the annotated headers ---------------
# Regenerates schema.json / ICD-007.md / icd-007.tex and compares against the
# working tree, ignoring the embedded generation timestamps. In-sync files are
# restored byte-for-byte so a green run leaves the tree untouched; stale files
# stay regenerated so the fix is one `git add` away.
schema_drift() {
    local gen=("${REPO}/interfaces/tools/generated/schema.json"
               "${REPO}/docs/interfaces/ICD-007-packet-payloads.md"
               "${REPO}/interfaces/tools/generated/icd-007.tex")
    local snap rc=0
    snap="$(mktemp -d)"
    local i=0
    for f in "${gen[@]}"; do
        cp "${f}" "${snap}/$((i++))" || return 1
    done
    "${REPO}/interfaces/tools/schemagen/run-schemagen.sh" >/dev/null || { rm -rf "${snap}"; return 1; }
    i=0
    for f in "${gen[@]}"; do
        if diff <(grep -v "Generated at\|generated (" "${snap}/${i}") \
                <(grep -v "Generated at\|generated (" "${f}") >/dev/null; then
            cp "${snap}/${i}" "${f}" # in sync - keep the old timestamp, tree stays clean
        else
            echo "STALE: ${f} did not match its annotations - regenerated, review and commit"
            rc=1
        fi
        i=$((i + 1))
    done
    rm -rf "${snap}"
    return "${rc}"
}

# --- interfaces/ contract headers compile standalone in both views --------
# flight view: arm gcc, gnu++23, annotations compiled out.
# reflection view (when clang-p2996 is installed): C++26, annotations active -
# the same mode schemagen uses, so a broken WIRE(...) fails here even if the
# schema was not regenerated.
iface_headers() {
    local inc="${REPO}/interfaces/include"
    local cxx="${CLANG_P2996_DIR:-/opt/compiler-explorer/clang-p2996}/bin/clang++"
    local tmp rc=0
    tmp="$(mktemp -d)"
    local headers
    mapfile -t headers < <(cd "${inc}" && find bolt -name '*.hpp' | sort)
    for h in "${headers[@]}"; do
        printf '#include <%s>\n' "${h}" > "${tmp}/tu.cpp"
        if ! arm-none-eabi-g++ -std=gnu++23 -fsyntax-only -I "${inc}" "${tmp}/tu.cpp"; then
            echo "flight view FAILED: ${h}"
            rc=1
        fi
        if [ -x "${cxx}" ] && ! "${cxx}" -std=c++26 -freflection -fannotation-attributes \
            -fexpansion-statements -stdlib=libc++ -DBOLT_SCHEMAGEN -fsyntax-only \
            -I "${inc}" "${tmp}/tu.cpp"; then
            echo "reflection view FAILED: ${h}"
            rc=1
        fi
    done
    [ -x "${cxx}" ] || echo "note: reflection view skipped (clang-p2996 not installed)"
    echo "checked ${#headers[@]} headers (flight + reflection view)"
    rm -rf "${tmp}"
    return "${rc}"
}

# --- ground tooling -------------------------------------------------------
rust_build() { (cd "${TT}" && cargo build --workspace --quiet); }
rust_tests() { (cd "${TT}" && cargo test --workspace --quiet); }
rust_fmt() { (cd "${TT}" && cargo fmt --all --check); }
rust_clippy() { (cd "${TT}" && cargo clippy --workspace --quiet -- -D warnings); }
ext_build() { (cd "${TT}/extension" && npm run --silent compile); }

# --- run ------------------------------------------------------------------
if [ "${DO_BUILD}" -eq 1 ]; then
    section "flight-build" flight_build
else
    skip "flight-build" "--no-build"
fi

section "host-tests" host_tests

if [ -x "${CLANG_P2996_DIR:-/opt/compiler-explorer/clang-p2996}/bin/clang++" ]; then
    section "schema-drift" schema_drift
else
    skip "schema-drift" "clang-p2996 not installed"
fi

if command -v arm-none-eabi-g++ >/dev/null; then
    section "iface-headers" iface_headers
else
    skip "iface-headers" "arm-none-eabi-g++ not found"
fi

if [ "${DO_RUST}" -eq 1 ]; then
    section "rust-build" rust_build
    section "rust-tests" rust_tests
    if (cd "${TT}" && cargo fmt --version >/dev/null 2>&1); then
        section "rust-fmt" rust_fmt
    else
        skip "rust-fmt" "rustfmt not installed"
    fi
    if (cd "${TT}" && cargo clippy --version >/dev/null 2>&1); then
        section "rust-clippy" rust_clippy
    else
        skip "rust-clippy" "clippy not installed"
    fi
else
    skip "rust-build" "--no-rust"
    skip "rust-tests" "--no-rust"
fi

if [ "${DO_EXT}" -eq 1 ]; then
    if [ -d "${TT}/extension/node_modules" ]; then
        section "ext-build" ext_build
    else
        skip "ext-build" "node_modules missing - run npm ci in telemetry-tools/extension"
    fi
else
    skip "ext-build" "--no-ext"
fi

if [ "${DO_LINT}" -eq 1 ]; then
    section "lint-flight" "${HERE}/lint-flight.sh"
else
    skip "lint-flight" "--no-lint"
fi

# --- summary --------------------------------------------------------------
echo
echo "===== summary ====="
for name in "${ORDER[@]}"; do
    printf '  %-14s %s\n' "${name}" "${RESULT[${name}]}"
done
if [ "${FAILED}" -ne 0 ]; then
    echo "verify: FAILED"
else
    echo "verify: OK"
fi
exit "${FAILED}"
