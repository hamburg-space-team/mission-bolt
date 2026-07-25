#!/usr/bin/env bash
# Regenerate the payload schema (+ ICD-007) from the WIRE(...)/PACKET(...)
# annotations in include/bolt/wire, via the C++26 reflection generator
# (schemagen.cpp) built with clang-p2996 (in the dev image).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)" # interfaces/tools/schemagen
IFACE="$(cd "${HERE}/../.." && pwd)"                 # interfaces/
REPO="$(cd "${IFACE}/.." && pwd)"                    # mission-bolt/
INCLUDE="${IFACE}/include"
OUT="${IFACE}/tools/generated/schema.json"
ICD="${REPO}/docs/interfaces/ICD-007-packet-payloads.md"
TEX="${IFACE}/tools/generated/icd-007.tex"
CXX_DIR="${CLANG_P2996_DIR:-/opt/compiler-explorer/clang-p2996}"

if [ ! -x "${CXX_DIR}/bin/clang++" ]; then
    echo "error: clang-p2996 not found at ${CXX_DIR} - run scripts/install-clang-p2996.sh" >&2
    echo "(the committed schema.json still builds; the compiler is only needed to regenerate it)" >&2
    exit 1
fi

LIB="${CXX_DIR}/lib/x86_64-unknown-linux-gnu"
BIN="$(mktemp -d)/schemagen"
# The compile also runs the annotation + packed-layout static_asserts; a missing
# WIRE(...) or a padding gap fails here.
"${CXX_DIR}/bin/clang++" -std=c++26 -freflection -fannotation-attributes -fexpansion-statements \
    -stdlib=libc++ -DBOLT_SCHEMAGEN -I "${INCLUDE}" \
    "${HERE}/schemagen.cpp" -o "${BIN}"
STAMP="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
LD_LIBRARY_PATH="${LIB}" "${BIN}" > "${OUT}"
LD_LIBRARY_PATH="${LIB}" "${BIN}" icd "${STAMP}" > "${ICD}"
LD_LIBRARY_PATH="${LIB}" "${BIN}" tex "${STAMP}" > "${TEX}"
echo "wrote ${OUT} + ${ICD} + ${TEX} via C++26 reflection (clang-p2996)"
