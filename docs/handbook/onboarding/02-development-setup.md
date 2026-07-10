# Onboarding -- Dev Setup

Reference page. You'll come back to it.

> **Since July 2026 we use devcontainers.** The container ships the
> whole flight-side toolchain pinned and preconfigured -- you don't
> install compilers, CMSIS-Toolbox or clangd by hand anymore. The
> manual setup below is kept as a fallback for people who can't run
> Docker.

## Devcontainer (default path)

**Prerequisites:**

- Docker (Docker Desktop on Windows/macOS, docker-ce on Linux;
  on Windows enable the WSL integration)
- VS Code + the **Dev Containers** extension
  (`ms-vscode-remote.remote-containers`)

**Bring-up:**

```bash
git clone https://github.com/hamburg-space-team/mission-bolt.git
```

Open the `mission-bolt` folder (the repo root -- the one containing
`.devcontainer/`) in VS Code, then `F1` -> **Dev Containers: Reopen
in Container**. Do NOT pick "Clone in Container Volume" -- the
workspace must stay a bind mount of your local clone (CubeMX on the
host writes into the same tree).

First start takes several minutes: the image build downloads the
toolchain, and `post-create.sh` pulls the CMSIS packs for all five
targets. Packs live on a named docker volume and survive rebuilds;
every later start takes seconds.

**What's inside** (pinned in `.devcontainer/Dockerfile`):

| Tool | Version |
| --- | --- |
| Arm GNU toolchain | 15.2.Rel1 |
| CMSIS-Toolbox | 2.14.1 |
| clangd / clang-format / clang-tidy | latest stable |
| g++-14, CMake (Kitware), Ninja | host tests, C++23 |
| OpenOCD, stlink-tools, gdb-multiarch, picocom | flash / debug / serial |

Build:

```bash
cbuild flight-software/bolt.csolution.yml --context btc.Debug+btc
```

Host tests:

```bash
cmake -S flight-software -B build/tests -DCMAKE_BUILD_TYPE=Debug -G Ninja
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```

**Flashing from inside the container:** the ST-Link reaches the
container via USB passthrough (WSL: `usbipd bind` once, `usbipd
attach --wsl` per plug-in) or via an external GDB server over TCP.
Both variants step-by-step:
[.devcontainer/README.md](../../../.devcontainer/README.md).

**STM32CubeMX stays on the host** (GUI + ST license). The workspace
is a bind mount, so regenerating on the host and building in the
container just works.

**Editor:** clangd, Cortex-Debug and the CMSIS extensions are
installed into the container automatically. The clangd-version
dance from the manual setup does not apply here.

## Ground Side (host, not containerized yet)

```bash
sudo apt install python3.12 python3.12-venv
curl -LsSf https://astral.sh/uv/install.sh | sh
curl -fsSL https://bun.sh/install | bash
```

`pip`/`npm` work as fallbacks. Team default is `uv` + `bun`.
Optional, for the Go variant of the sensor-api:
`sudo apt install golang-go`.

```bash
cd ground-station
uv sync
source .venv/bin/activate

cd sensor-dashboard
bun install
```

Full bring-up: [running-ground-station](../../guides/running-ground-station.md).


## Troubleshooting

Ask in `#bolt_software`.

## Next

[03-first-contribution.md](03-first-contribution.md).
