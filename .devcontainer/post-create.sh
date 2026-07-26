#!/usr/bin/env bash
set -euo pipefail

# CI runners remap the container user's uid (1001 vs the image's 1000), so
# the packs volume and the baked cargo/rustup dirs belong to someone else
# there. Take ownership of whatever we need to write
for d in "${CMSIS_PACK_ROOT:-/opt/cmsis-packs}" /usr/local/cargo /usr/local/rustup; do
    if [ -d "${d}" ] && [ ! -w "${d}" ]; then
        sudo chown -R "$(id -u):$(id -g)" "${d}"
    fi
done

if [ ! -f "${CMSIS_PACK_ROOT}/.Web/index.pidx" ]; then
    cpackget init https://www.keil.com/pack/index.pidx
fi

mapfile -t target_types < <(
    cbuild list contexts flight-software/bolt.csolution.yml \
        | sed 's/.*+//' | sort -u
)

for tt in "${target_types[@]}"; do
    echo "== setup ${tt} =="
    cbuild setup flight-software/bolt.csolution.yml --active "${tt}" --packs
done

# Regenerate the payload schema from the bolt/wire WIRE(...) annotations so
# bolt-codec's build.rs picks up any change (C++26 reflection via clang-p2996).
if [ -f interfaces/tools/schemagen/run-schemagen.sh ]; then
    echo "== regenerating payload schema.json =="
    interfaces/tools/schemagen/run-schemagen.sh \
        || echo "schemagen: skipped (schema.json kept as committed)"
fi

# --- telemetry-tools (Bolt): Rust codec/CLIs + VS Code extension ---
if [ -d telemetry-tools ]; then
    echo "== building telemetry-tools (Bolt) =="
    ( cd telemetry-tools && cargo build ) \
        || ( echo "telemetry-tools: build with hdf5 failed (check libhdf5) - building without hdf5"; \
             cd telemetry-tools && cargo build --no-default-features )
    bash scripts/rebuild-extension.sh \
        || echo "telemetry-tools: extension build/install failed - run scripts/rebuild-extension.sh by hand"
fi

echo "Devcontainer ready. Build with:"
echo "  cbuild flight-software/bolt.csolution.yml --context btc.Debug+btc"