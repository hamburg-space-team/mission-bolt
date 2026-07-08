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

echo "Devcontainer ready. Build with:"
echo "  cbuild flight-software/bolt.csolution.yml --context btc.Debug+btc"