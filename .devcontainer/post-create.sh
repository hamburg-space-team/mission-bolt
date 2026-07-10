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

echo "Devcontainer ready. Build with:"
echo "  cbuild flight-software/bolt.csolution.yml --context btc.Debug+btc"