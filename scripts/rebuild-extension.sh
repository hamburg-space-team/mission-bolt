#!/usr/bin/env bash
# Rebuild the boltscope VS Code extension and (re)install it into the editor.
# Works inside the devcontainer (code-server) and on desktop VS Code (code CLI).
# After a successful install, reload the window: "Developer: Reload Window".
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXT="$(cd "${HERE}/../telemetry-tools/extension" && pwd)"
cd "${EXT}"

[ -d node_modules ] || npm install

# gen-commands + tsc (host & webview) + vite build
npm run compile

# same invocation as package.json's "package" script; the --require shim only
# matters on Node < 20 and is a no-op otherwise
node --require ./scripts/file-global.cjs node_modules/@vscode/vsce/vsce \
    package --no-dependencies -o bolt.vsix

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
