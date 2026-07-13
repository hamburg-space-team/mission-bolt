#!/usr/bin/env bash
# Install the clang-p2996 fork (C++26 reflection P2996 + annotations P3394) so schemagen can run.

set -euo pipefail

DEST="${CLANG_P2996_DEST:-/opt/compiler-explorer}"
BUCKET="https://s3.amazonaws.com/compiler-explorer"

# Newest clang-bb-p2996 nightly tarball on the CE bucket.
KEY="$(curl -fsSL "${BUCKET}/?list-type=2&prefix=opt/clang-bb-p2996-trunk-" \
       | grep -oE 'opt/clang-bb-p2996-trunk-[0-9]+\.tar\.xz' | sort | tail -1)"
[ -n "${KEY}" ] || { echo "no clang-bb-p2996 tarball found on ${BUCKET}"; exit 1; }
NAME="$(basename "${KEY}" .tar.xz)"

if [ ! -x "${DEST}/${NAME}/bin/clang++" ]; then
    echo "downloading ${KEY} (~1 GB, streamed extract)..."
    sudo mkdir -p "${DEST}"
    sudo chown "$(id -u):$(id -g)" "${DEST}"
    curl -fsSL "${BUCKET}/${KEY}" | tar -xJ -C "${DEST}"
fi

# Stable, date-free path for scripts.
ln -sfn "${DEST}/${NAME}" "${DEST}/clang-p2996"
echo "installed: ${DEST}/clang-p2996/bin/clang++ ($("${DEST}/${NAME}/bin/clang++" --version | head -1))"
echo "regenerate the schema with:  interfaces/tools/schemagen/run-schemagen.sh"
