#!/usr/bin/env bash
# Mechanical checks for two properties review used to carry alone
# (docs/standards/ai/tool-classification.md names them as the gaps):
#
#   alloc   the flight images are allocator-free and stay that way.
#           arm-none-eabi-nm over out/**/*.elf rejects allocating
#           operator new AND any newlib heap symbol (malloc/free/sbrk).
#           This holds because littlefs runs on static buffers with
#           LFS_NO_MALLOC, its log/assert paths trap instead of printf
#           (shared/utils/assert_trap.cpp), and operator delete is
#           replaced with a trap - the deleting-destructor artefacts
#           would otherwise pull free via libstdc++.
#
#   layers  mission logic must not include board or vendor HAL headers.
#           Hard-clean zone: btc/app/btc, btc/app/protocol. The shared/
#           tree is ratcheted: the platform-facing files listed below may
#           include HAL, anything new that does fails.
#
#   --selftest   prove both checks fail on known-bad input before
#                anything trusts them (the fail-first rule of ADR-013)
#
# usage: check-flight-invariants.sh [alloc|layers|--selftest]  (default: both)

set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FSW="$(cd "${HERE}/../flight-software" && pwd)"

# Allocating operator new / new[] (plain, aligned, nothrow) in 32/64-bit
# manglings. Placement new (_Zn..Pv) allocates nothing and is exempt: the
# optional/expected machinery emits it as a weak symbol
NEW_RE=' _Zn[wa][jm]($|St11align_val_t|RKSt9nothrow_t)'
HEAP_RE=' (_malloc_r|malloc|_free_r|free|_sbrk_r|_sbrk|_calloc_r|calloc|_realloc_r|realloc)$'
HAL_RE='#include[^"<]*["<](main\.h|stm32[^">]*|cmsis_os[^">]*)[">]'

# shared/ files allowed to touch HAL: the platform-facing side of the
# shared layer. Removing an entry is progress; adding one needs a reason
SHARED_ALLOW=(
    "shared/core/boot_state.cpp"      # RCC reset flags
    "shared/core/exp_computer.cpp"    # main.h, IWYU-kept
    "shared/sensors/icm42686.cpp"     # HAL delay
    "shared/sensors/icm42688.cpp"     # HAL delay
    "shared/storage/sd_store.cpp"     # SDMMC handle
    "shared/utils/crc16_hw.cpp"       # CRC peripheral
    "shared/timing/timing.hpp"        # DWT cycle counter
    "shared/comms/bxcan_transport.cpp" # bxCAN impl of CanTransport
)

check_alloc() {
    local rc=0 elfs
    mapfile -t elfs < <(find "${FSW}/out" -name '*.elf' 2>/dev/null | sort)
    if [ "${#elfs[@]}" -eq 0 ]; then
        echo "alloc: no .elf found - build first (cbuild, or verify.sh flight-build)"
        return 1
    fi
    for e in "${elfs[@]}"; do
        local syms
        syms="$(arm-none-eabi-nm "${e}")"
        if grep -qE "${NEW_RE}" <<<"${syms}"; then
            echo "alloc: FAIL ${e#"${FSW}"/}: operator new in the image (I-3)"
            grep -E "${NEW_RE}" <<<"${syms}" | sed 's/^/  /'
            rc=1
        fi
        if grep -qE "${HEAP_RE}" <<<"${syms}"; then
            echo "alloc: FAIL ${e#"${FSW}"/}: newlib heap in the image (I-3)"
            grep -E "${HEAP_RE}" <<<"${syms}" | sed 's/^/  /'
            echo "  a printf/assert path or an allocating call slipped in; see assert_trap.cpp + the LFS_NO_* defines"
            rc=1
        fi
    done
    [ "${rc}" = 0 ] && echo "alloc: ${#elfs[@]} images allocator-free"
    return "${rc}"
}

# layer_scan <root> <file...> -> prints offending files
layer_scan() {
    local root="$1"; shift
    (cd "${root}" && grep -lE "${HAL_RE}" "$@" 2>/dev/null || true)
}

check_layers() {
    local rc=0

    local clean
    mapfile -t clean < <(cd "${FSW}" && find btc/app/btc btc/app/protocol -name '*.[ch]pp')
    local hits
    hits="$(layer_scan "${FSW}" "${clean[@]}")"
    if [ -n "${hits}" ]; then
        echo "layers: FAIL - HAL include in mission logic:"
        sed 's/^/  /' <<<"${hits}"
        rc=1
    fi

    local shared
    mapfile -t shared < <(cd "${FSW}" && find shared -name '*.[ch]pp' -not -path 'shared/lib/*')
    while IFS= read -r f; do
        [ -z "${f}" ] && continue
        local allowed=0
        for a in "${SHARED_ALLOW[@]}"; do
            [ "${f}" = "${a}" ] && allowed=1
        done
        if [ "${allowed}" = 0 ]; then
            echo "layers: FAIL - new HAL include in shared/: ${f}"
            echo "  either move the HAL use behind the platform seam or allowlist it with a reason"
            rc=1
        fi
    done <<<"$(layer_scan "${FSW}" "${shared[@]}")"

    # ratchet hygiene: an allowlisted file that no longer needs it should leave
    for a in "${SHARED_ALLOW[@]}"; do
        if [ -f "${FSW}/${a}" ] && ! grep -qE "${HAL_RE}" "${FSW}/${a}"; then
            echo "layers: note - ${a} is allowlisted but clean; remove it from SHARED_ALLOW"
        fi
    done

    [ "${rc}" = 0 ] && echo "layers: mission logic clean, shared/ ratchet holds (${#SHARED_ALLOW[@]} allowlisted)"
    return "${rc}"
}

selftest() {
    local ok=1 tmp
    tmp="$(mktemp -d)"

    # known-bad ELF symbol table: operator new present -> must be caught
    if ! grep -qE "${NEW_RE}" <<<"08001234 T _Znwm"; then
        echo "SELFTEST FAIL: operator new fixture not detected"
        ok=0
    fi
    # known-good: deleting-destructor artifact must NOT be flagged
    if grep -qE "${NEW_RE}" <<<"08001234 T _ZdlPv"; then
        echo "SELFTEST FAIL: operator delete artifact wrongly flagged"
        ok=0
    fi
    # known-good: placement new allocates nothing and must NOT be flagged
    if grep -qE "${NEW_RE}" <<<"08001234 W _ZnwjPv"; then
        echo "SELFTEST FAIL: placement new wrongly flagged"
        ok=0
    fi
    # known-bad: newlib heap symbol -> must be caught
    if ! grep -qE "${HEAP_RE}" <<<"08001234 T _malloc_r"; then
        echo "SELFTEST FAIL: newlib malloc fixture not detected"
        ok=0
    fi
    # known-good: plain libc string routine must NOT be flagged
    if grep -qE "${HEAP_RE}" <<<"08001234 T memcpy"; then
        echo "SELFTEST FAIL: memcpy wrongly flagged as heap"
        ok=0
    fi
    # known-bad source file: vendor HAL include -> must be caught
    printf '#include "stm32l4xx_hal.h"\n' > "${tmp}/bad.cpp"
    printf '#include <array>\n' > "${tmp}/good.cpp"
    local found
    found="$(layer_scan "${tmp}" bad.cpp good.cpp)"
    if [ "${found}" != "bad.cpp" ]; then
        echo "SELFTEST FAIL: HAL include fixture not detected (got: '${found}')"
        ok=0
    fi

    rm -rf "${tmp}"
    if [ "${ok}" = 1 ]; then
        echo "invariants selftest: both checks reject their known-bad inputs"
        return 0
    fi
    return 1
}

case "${1:-all}" in
--selftest) selftest ;;
alloc) check_alloc ;;
layers) check_layers ;;
all)
    rc=0
    check_alloc || rc=1
    check_layers || rc=1
    exit "${rc}"
    ;;
*)
    echo "usage: check-flight-invariants.sh [alloc|layers|--selftest]"
    exit 2
    ;;
esac
