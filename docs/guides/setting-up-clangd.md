# Setting Up clangd

clangd is our language server for the flight code -- completion,
go-to-definition, diagnostics, inline clang-tidy. The flight code is
C++23 (`std::expected`, `gnu++23`) and the cross-compile uses
ARM-specific flags clang doesn't recognise out of the box. The setup
has a few sharp edges; this page captures all of them.

## Minimum version

**clangd >= 22.x** (Apple-numbered clangd from clangd/clangd
releases; equivalent to mainline LLVM >= 17 for our purposes).

Why so new:

- `std::expected` is C++23. clang 14--16 don't parse it at all; clang
  17--19 parse it but mis-diagnose constexpr cases the test suite
  hits.
- `gnu++23` as a `-std=` value is only accepted by clang >= 17.
  Older clang fails with `invalid value 'gnu++23'`. The actual
  flight build uses `-std=gnu++23` (see `bolt.csolution.yml`), so
  clangd has to accept it.
- The bundled clangd in some VS Code C/C++ extensions is older than
  the system clangd. Don't trust either by default; pin the path.

Ubuntu 22.04 (jammy) ships at most **clangd-15**. That is too old.
You need a newer one explicitly installed or downloaded.

## Install (Ubuntu, WSL, macOS)

Easiest path: download the prebuilt binary release. No sudo needed.

```bash
mkdir -p ~/.local/clangd
cd /tmp
curl -L -o clangd.zip https://github.com/clangd/clangd/releases/download/22.1.6/clangd-linux-22.1.6.zip
unzip -q clangd.zip
mv clangd_22.1.6 ~/.local/clangd/
~/.local/clangd/clangd_22.1.6/bin/clangd --version
# expect: clangd version 22.1.x
```

Alternative (system-wide via LLVM apt repo, needs sudo):

```bash
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
echo 'deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-19 main' | \
  sudo tee /etc/apt/sources.list.d/llvm-19.list
sudo apt update && sudo apt install -y clangd-19
```

LLVM 19+ works for both `<expected>` and `gnu++23`. The version
that ships with the OS does not.

## VS Code: point at the right binary

The official `llvm-vs-code-extensions.vscode-clangd` extension auto-
discovers `clangd` on the PATH, which on Ubuntu 22.04 picks up the
too-old one. Override it explicitly.

User settings (`~/.vscode-server/data/Machine/settings.json`):

```jsonc
{
    "clangd.path": "/home/<you>/.local/clangd/clangd_22.1.6/bin/clangd"
}
```

If you installed via apt: `"clangd.path": "/usr/bin/clangd-19"`.

The flight folder's `.vscode/settings.json` repeats the same key for
people who open the folder directly without the workspace. Keep them
in sync, or rely on the user-level one.

## The merge step

The top-level `compile_commands.json` is built by the **Merge
compile_commands.json** task (`tasks.json`). It walks
`out/**/compile_commands.json` and `build/**/compile_commands.json`
and concatenates them.

Run it whenever you add a new translation unit or rebuild a context
that wasn't previously merged. Most day-to-day edits don't need a
re-merge -- changing the contents of an existing `.cpp` doesn't
change its compile flags.


## Verify the setup

```bash
~/.local/clangd/clangd_22.1.6/bin/clangd \
  --check=flight-software/shared/core/exp_computer.cpp \
  --background-index
```

Expected last line: `All checks completed, 0 errors`. If you see
`pp_file_not_found` or `invalid value 'gnu++23'`, the clangd binary
is too old or the compile_commands chain is misrouted.

In VS Code: open Output panel, switch the dropdown to **clangd**.
The first line names the binary it's running. Confirm it matches
your `clangd.path` setting and the version is 22.x. If you see two
language servers in the dropdown (clangd + C/C++), the cpptools
disable in `settings.json` didn't take.

## References

- [cpp.md](../standards/coding/cpp.md) -- the coding standard
- [building-flight-software.md](building-flight-software.md) -- where
  the per-target `compile_commands.json` files come from
- [running-flight-unit-tests.md](running-flight-unit-tests.md) --
  the host build that generates `build/tests/compile_commands.json`
