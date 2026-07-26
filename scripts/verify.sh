#!/usr/bin/env bash
# One-stop verification for mission-bolt. Run it after any change - it is the
# same gate for everyone (and for CI): flight builds, host unit tests,
# generated-artifact drift, flight invariants, ground tooling, provenance,
# lint.
#
#   scripts/verify.sh                    full gate
#   scripts/verify.sh --list             list the stage names
#   scripts/verify.sh host-tests lint-flight   run only the named stages
#   scripts/verify.sh --no-lint          full gate without the lint pass
#
# Skip flags: --no-build --no-rust --no-ext --no-lint --no-prov
#
# Stages report PASS/FAIL/SKIP; the script exits non-zero if anything FAILs.
# SKIPs (missing optional tooling) never fail the gate, but are listed.
# New checks carry a --selftest that proves they fail on known-bad input;
# the stage runs the selftest first (docs/standards/ai/policy.md).
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "${HERE}/.." && pwd)"
FSW="${REPO}/flight-software"
TT="${REPO}/telemetry-tools"

STAGES=(flight-build host-tests coverage schema-drift iface-headers invariants
    rust-build rust-tests rust-fmt rust-clippy ext-build provenance lint-flight)

DO_LINT=1 DO_BUILD=1 DO_RUST=1 DO_EXT=1 DO_PROV=1
ONLY=()
for arg in "$@"; do
    case "${arg}" in
    --list) printf '%s\n' "${STAGES[@]}"; exit 0 ;;
    --no-lint) DO_LINT=0 ;;
    --no-build) DO_BUILD=0 ;;
    --no-rust) DO_RUST=0 ;;
    --no-ext) DO_EXT=0 ;;
    --no-prov) DO_PROV=0 ;;
    -*) echo "unknown option: ${arg} (--list shows stages)"; exit 2 ;;
    *)
        if ! printf '%s\n' "${STAGES[@]}" | grep -qx "${arg}"; then
            echo "unknown stage: ${arg} (--list shows stages)"
            exit 2
        fi
        ONLY+=("${arg}")
        ;;
    esac
done

# wants <stage> - true when the stage is selected (default: all)
wants() {
    [ "${#ONLY[@]}" -eq 0 ] && return 0
    printf '%s\n' "${ONLY[@]}" | grep -qx "$1"
}

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

# --- host unit tests (Catch2 via ctest, ASan+UBSan on by default) ---------
host_tests() {
    local bld="${FSW}/build/tests"
    cmake -S "${FSW}" -B "${bld}" >/dev/null &&
        cmake --build "${bld}" >/dev/null &&
        ctest --test-dir "${bld}" --output-on-failure
}

# --- host-test coverage ----------------------------------------------------
# Separate build tree with gcov instrumentation (sanitizers off, they skew
# the counters). The test RUN is the hard gate; the line numbers are honesty
# about what the suites reach, never a threshold to game
coverage_check() {
    local bld="${FSW}/build/coverage"
    cmake -S "${FSW}" -B "${bld}" -DBOLT_SANITIZE=OFF -DBOLT_COVERAGE=ON >/dev/null &&
        cmake --build "${bld}" >/dev/null &&
        ctest --test-dir "${bld}" --output-on-failure >/dev/null || return 1
    local gcovdir
    gcovdir="$(mktemp -d)"
    (cd "${gcovdir}" && find "${bld}" -name '*.gcda' -exec gcov {} + 2>/dev/null) |
        awk -v repo="${REPO}/" '
            /^File /  { f = $2; gsub(/\x27/, "", f); sub(repo, "", f) }
            /^Lines executed/ {
                split($0, a, /:| of /)
                pct = a[2]; n = a[3]
                if (f ~ /flight-software\/(shared|tests)\//) {
                    printf "  %6s of %4d lines  %s\n", pct, n, f
                    covered += (substr(pct, 1, length(pct)-1) / 100) * n
                    total += n
                }
            }
            END {
                if (total > 0) printf "  total: %.1f%% of %d lines in shared/ + tests/\n",
                    100 * covered / total, total
            }'
    echo "  note: counts only code the host tests link at all - most of shared/"
    echo "  (core, comms, sensors) is not host-linkable yet and appears nowhere here"
    rm -rf "${gcovdir}"
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

# --- flight invariants with a mechanical check ----------------------------
# selftest first: a check is only trusted after it rejected known-bad input
invariants_check() {
    "${HERE}/check-flight-invariants.sh" --selftest &&
        "${HERE}/check-flight-invariants.sh"
}

# --- provenance of assisted contributions ---------------------------------
# Disclosure is a property of the repository, not a habit; a branch that
# bypassed the local hook fails here. See docs/standards/ai/provenance.md
provenance_check() {
    "${HERE}/check-provenance.sh" --selftest &&
        "${HERE}/check-provenance.sh" "${PROVENANCE_RANGE:-origin/main..HEAD}"
}

# --- ground tooling -------------------------------------------------------
rust_build() { (cd "${TT}" && cargo build --workspace --quiet); }
rust_tests() { (cd "${TT}" && cargo test --workspace --quiet); }
rust_fmt() { (cd "${TT}" && cargo fmt --all --check); }
rust_clippy() { (cd "${TT}" && cargo clippy --workspace --quiet -- -D warnings); }
ext_build() { (cd "${TT}/extension" && { [ -d node_modules ] || npm ci; } && npm run --silent compile); }

# --- run ------------------------------------------------------------------
if wants flight-build; then
    if [ "${DO_BUILD}" -eq 1 ]; then
        section "flight-build" flight_build
    else
        skip "flight-build" "--no-build"
    fi
fi

wants host-tests && section "host-tests" host_tests
wants coverage && section "coverage" coverage_check

if wants schema-drift; then
    if [ -x "${CLANG_P2996_DIR:-/opt/compiler-explorer/clang-p2996}/bin/clang++" ]; then
        section "schema-drift" schema_drift
    else
        skip "schema-drift" "clang-p2996 not installed"
    fi
fi

if wants iface-headers; then
    if command -v arm-none-eabi-g++ >/dev/null; then
        section "iface-headers" iface_headers
    else
        skip "iface-headers" "arm-none-eabi-g++ not found"
    fi
fi

if wants invariants; then
    if command -v arm-none-eabi-nm >/dev/null; then
        section "invariants" invariants_check
    else
        skip "invariants" "arm-none-eabi-nm not found"
    fi
fi

if [ "${DO_RUST}" -eq 1 ]; then
    wants rust-build && section "rust-build" rust_build
    wants rust-tests && section "rust-tests" rust_tests
    if wants rust-fmt; then
        if (cd "${TT}" && cargo fmt --version >/dev/null 2>&1); then
            section "rust-fmt" rust_fmt
        else
            skip "rust-fmt" "rustfmt not installed"
        fi
    fi
    if wants rust-clippy; then
        if (cd "${TT}" && cargo clippy --version >/dev/null 2>&1); then
            section "rust-clippy" rust_clippy
        else
            skip "rust-clippy" "clippy not installed"
        fi
    fi
else
    wants rust-build && skip "rust-build" "--no-rust"
    wants rust-tests && skip "rust-tests" "--no-rust"
fi

if wants ext-build; then
    if [ "${DO_EXT}" -eq 0 ]; then
        skip "ext-build" "--no-ext"
    else
        section "ext-build" ext_build
    fi
fi

if wants provenance; then
    if [ "${DO_PROV}" -eq 1 ]; then
        section "provenance" provenance_check
    else
        skip "provenance" "--no-prov"
    fi
fi

if wants lint-flight; then
    if [ "${DO_LINT}" -eq 1 ]; then
        section "lint-flight" "${HERE}/lint-flight.sh"
    else
        skip "lint-flight" "--no-lint"
    fi
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
