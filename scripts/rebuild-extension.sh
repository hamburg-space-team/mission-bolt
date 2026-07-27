#!/usr/bin/env bash
# Rebuild the boltscope VS Code extension and (re)install it into the editor.
# Works inside the devcontainer (code-server) and on desktop VS Code (code CLI).
# After a successful install, reload the window: "Developer: Reload Window".
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXT="$(cd "${HERE}/../telemetry-tools/extension" && pwd)"
REPO="$(cd "${HERE}/.." && pwd)"
cd "${EXT}"

# Skip the build when bolt.vsix is newer than everything that feeds it -
# a container rebuild then only reinstalls (seconds instead of a minute).
# --force rebuilds regardless
build_needed() {
    [ "${1:-}" = "--force" ] && return 0
    [ -f bolt.vsix ] || return 0
    [ -n "$(find src webview package.json "${REPO}/interfaces/tools/generated/schema.json" \
        -newer bolt.vsix -print -quit 2>/dev/null)" ]
}

if build_needed "${1:-}"; then
    [ -d node_modules ] || npm install
    # gen-commands + tsc (host & webview) + vite build
    npm run compile
    # the webview resolves its CSS through the vite manifest at runtime; a
    # too-eager .vscodeignore once shipped a style-less build
    if ! node node_modules/@vscode/vsce/vsce ls --no-dependencies \
        | grep -q 'media/webview/.vite/manifest.json'; then
        echo "FAIL: package would ship without the vite manifest (webview styles break)"
        exit 1
    fi
    # same invocation as package.json's "package" script
    node node_modules/@vscode/vsce/vsce package --no-dependencies -o bolt.vsix
else
    echo "bolt.vsix up to date - skipping build (--force to rebuild)"
fi

# install: prefer the devcontainer's code-server, fall back to the code CLI
ext_dir="${HOME}/.vscode-server/extensions"
code_server="$(ls -1 "${HOME}"/.vscode-server/bin/*/bin/code-server \
    /vscode/vscode-server/bin/*/bin/code-server 2>/dev/null | head -1 || true)"

if [ -n "${code_server}" ]; then
    "${code_server}" --install-extension "${EXT}/bolt.vsix" \
        --extensions-dir "${ext_dir}" --force
    echo "installed via code-server - reload the VS Code window to pick it up"
elif command -v code >/dev/null; then
    code --install-extension "${EXT}/bolt.vsix" --force
    echo "installed via code CLI - reload the VS Code window to pick it up"
else
    echo "no VS Code CLI found - install ${EXT}/bolt.vsix by hand:"
    echo "  Extensions view -> ... -> Install from VSIX"
fi
