# Onboarding -- Dev Setup

Reference page. You'll come back to it.

## OS

Anything works for host-side tests (Linux, macOS, Windows + WSL).
For flashing real boards, Linux or macOS are smoother. Examples
below use Ubuntu; translate as needed.

## Flight Side

**Cross-compiler:**

```bash
sudo apt install gcc-arm-none-eabi
arm-none-eabi-gcc --version       # want 13.3.1
```

If your distro's package is older, grab the official build from
ARM. See [ADR-006](../../decisions/ADR-006-cmsis-toolbox.md).

**CMSIS-Toolbox 2.12.0** -- follow the upstream release notes.
Verify `cbuild --version`.

**Host tests:**

```bash
sudo apt install cmake g++ python3
```

**Flashing / debug:**

```bash
sudo apt install openocd gdb-multiarch
```

On Linux, install the OpenOCD udev rules so you can flash without
root, then add yourself to `plugdev` / `dialout`. Log out and back
in.

## Ground Side

```bash
sudo apt install python3.12 python3.12-venv
curl -LsSf https://astral.sh/uv/install.sh | sh
curl -fsSL https://bun.sh/install | bash
```

`pip`/`npm` work as fallbacks. Team default is `uv` + `bun`.

Optional, for the Go variant of the sensor-api: `sudo apt install golang-go`.

## Editor

VS Code is the default. Extensions worth installing:

- clangd, CMake Tools, Cortex-Debug
- ESLint, Python, Pylance

The `flight-software/.vscode/` folder ships tasks for build/flash/
format/test. Open the `flight-software` folder as a workspace.

Other editors are fine. clangd reads the merged
`compile_commands.json` no matter the wrapper.

### clangd version

We use C++23 (`std::expected`, `gnu++23`). The Ubuntu 22.04 system
`clangd` (version 14) and the `clangd-15` package are **too old** --
they don't parse `std::expected` or accept `gnu++23`. You need
**clangd >= 22.x** or LLVM >= 17.

Quick install (no sudo):

```bash
mkdir -p ~/.local/clangd && cd /tmp && \
  curl -L -o clangd.zip https://github.com/clangd/clangd/releases/download/22.1.6/clangd-linux-22.1.6.zip && \
  unzip -q clangd.zip && mv clangd_22.1.6 ~/.local/clangd/
```

Then set `clangd.path` in your VS Code settings to
`~/.local/clangd/clangd_22.1.6/bin/clangd`.

Full setup, troubleshooting, and the per-folder `.clangd` routing
are in [setting-up-clangd.md](../../guides/setting-up-clangd.md).
Read it before opening the project in VS Code -- you'll lose a day
hunting `'expected' file not found` and `invalid value 'gnu++23'`
otherwise.

## Clone and First Build

```bash
git clone https://github.com/hamburg-space-team/mission-bolt.git
cd mission-bolt/flight-software
```

Host tests first (no CMSIS packs needed):

```bash
cmake -S . -B build/tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```

Then flight:

```bash
cpackget update-index
cpackget add ARM::CMSIS@6.3.0 ARM::CMSIS-Compiler@2.2.0 \
             ARM::CMSIS-Driver_STM32@1.3.0 Keil::STM32L4xx_DFP@3.1.0
cbuild bolt.csolution.yml --context btc.Debug+btc
```

See [building-flight-software](../../guides/building-flight-software.md)
for the other targets.

## Ground Station

```bash
cd ground-station
uv sync
source .venv/bin/activate

cd sensor-dashboard
bun install
```

Full bring-up: [running-ground-station](../../guides/running-ground-station.md).

## Smoke Test

You're good when:

1. `cbuild ... btc.Debug+btc` produces `out/btc/btc/Debug/btc.elf`.
2. `ctest --test-dir build/tests` is green.
3. clangd shows no red squiggles in
   [`crc16.hpp`](../../../flight-software/shared/utils/crc16.hpp).
4. The synthetic-source + sensor-api + dashboard chain renders live
   data in a browser.

## Troubleshooting

Aask in `#bolt_software`.

## Next

[03-first-contribution.md](03-first-contribution.md).
