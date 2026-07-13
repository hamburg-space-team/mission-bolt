#!/usr/bin/env bash
set -euo pipefail

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

# Share Claude Code memory
if [ -n "${HOST_PROJECT_DIR:-}" ] && [ -d "$HOME/.claude/projects" ]; then
    host_slug="$(printf '%s' "$HOST_PROJECT_DIR" | tr '/.' '--')"
    container_slug="$(printf '%s' "$PWD" | tr '/.' '--')"
    if [ -d "$HOME/.claude/projects/$host_slug" ] && [ ! -e "$HOME/.claude/projects/$container_slug" ]; then
        ln -s -- "$host_slug" "$HOME/.claude/projects/$container_slug"
    fi
fi

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
    ( cd telemetry-tools/extension && npm install && npm run compile ) \
        || echo "telemetry-tools: extension build failed"

    ext_dir="$HOME/.vscode-server/extensions"
    code_server="$(ls -1 "$HOME"/.vscode-server/bin/*/bin/code-server \
        /vscode/vscode-server/bin/*/bin/code-server 2>/dev/null | head -1)"
    if [ -n "$code_server" ] && \
       ( cd telemetry-tools/extension && npx --yes @vscode/vsce package --no-dependencies -o bolt.vsix ); then
        "$code_server" --install-extension "$PWD/telemetry-tools/extension/bolt.vsix" \
            --extensions-dir "$ext_dir" --force \
            || echo "telemetry-tools: auto-install failed - build the extension and reload the window"
    else
        echo "telemetry-tools: code-server/vsce unavailable - build the extension and reload the window"
    fi
fi

echo "Devcontainer ready. Build with:"
echo "  cbuild flight-software/bolt.csolution.yml --context btc.Debug+btc"