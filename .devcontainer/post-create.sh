#!/usr/bin/env bash
set -euo pipefail

# CI runners remap the container user's uid (1001 vs the image's 1000), and
# fresh named volumes arrive root-owned. Take ownership of whatever we write
for d in "${CMSIS_PACK_ROOT:-/opt/cmsis-packs}" /usr/local/cargo /usr/local/rustup \
    /usr/local/cargo/registry "${HOME}/.npm"; do
    if [ -d "${d}" ] && [ ! -w "${d}" ]; then
        sudo chown -R "$(id -u):$(id -g)" "${d}"
    fi
done

# littlefs is a submodule pinned to one commit; without it there is no lfs.c
git submodule update --init

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
# Only when a wire header is newer than the committed schema - the verify
# gate's drift stage catches anything this heuristic misses
if [ -n "$(find interfaces/include/bolt -name '*.hpp' \
    -newer interfaces/tools/generated/schema.json -print -quit 2>/dev/null)" ]; then
    echo "== regenerating payload schema.json =="
    interfaces/tools/schemagen/run-schemagen.sh \
        || echo "schemagen: skipped (schema.json kept as committed)"
else
    echo "== schema.json current - skipping schemagen"
fi

# Under devcontainers/ci (GITHUB_ENV is plumbed in) the verify gate builds
# codec, tooling and extension itself right after this script - rebuilding
# them here would be the same work twice
if [ -n "${GITHUB_ENV:-}" ]; then
    echo "== CI - packs are set up, the verify gate builds the rest"
    exit 0
fi

# --- telemetry-tools (Bolt): Rust codec/CLIs + VS Code extension ---
if [ -d telemetry-tools ]; then
    # skip even cargo's freshness check when the bridge binary is newer
    # than every input (same idea as the bolt.vsix guard)
    if [ -f telemetry-tools/target/debug/bolt-serial-bridge ] &&
        [ -z "$(find telemetry-tools/crates telemetry-tools/Cargo.toml telemetry-tools/Cargo.lock \
            interfaces/tools/generated/schema.json \
            -newer telemetry-tools/target/debug/bolt-serial-bridge -print -quit 2>/dev/null)" ]; then
        echo "== telemetry-tools binaries current - skipping cargo build"
    else
        echo "== building telemetry-tools (Bolt) =="
        ( cd telemetry-tools && cargo build ) \
            || ( echo "telemetry-tools: build with hdf5 failed (check libhdf5) - building without hdf5"; \
                 cd telemetry-tools && cargo build --no-default-features )
    fi
    bash scripts/rebuild-extension.sh \
        || echo "telemetry-tools: extension build/install failed - run scripts/rebuild-extension.sh by hand"
fi

echo "Devcontainer ready. Build with:"
echo "  cbuild flight-software/bolt.csolution.yml --context btc.Debug+btc"