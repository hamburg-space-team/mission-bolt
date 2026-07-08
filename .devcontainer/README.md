# Devcontainer: mission-bolt flight software

Reproducible toolchain for every team member (versions as of 2026-07):

| Tool | Version | Purpose |
|---|---|---|
| Arm GNU toolchain | 15.2.Rel1 (gcc 15.2.1) | target firmware builds |
| CMSIS-Toolbox | 2.14.1 | cbuild/csolution/cpackget |
| CMake (Kitware repo) + Ninja | latest | build backend |
| LLVM clang-format/tidy/clangd | latest stable (apt.llvm.org) | lint/format/IDE |
| g++-14 | Ubuntu 24.04 | host unit tests (C++23) |
| OpenOCD + stlink-tools + gdb-multiarch | apt | flash & debug |
| picocom + python3-serial | apt | downlink/serial dumps |
| doxygen + graphviz | apt | API docs |

CMSIS packs live on a named docker volume and survive rebuilds.
STM32CubeMX/CubeProgrammer stay on the Windows host (GUI, ST license) -
OpenOCD covers flashing and debugging inside the container.

## Build

```sh
cbuild flight-software/bolt.csolution.yml --context btc.Debug+btc
# contexts: btc.Debug+btc, exp1.Debug+exp1-space-disco,
#           exp2.Debug+exp2-bouncy-castle, exp3.Debug+exp3-floaty-boi
```

Host unit tests:

```sh
cmake -S flight-software/tests -B build/tests -G Ninja && cmake --build build/tests && ctest --test-dir build/tests
```

## ST-Link, variant A: USB passthrough (recommended on WSL2)

One-time on **Windows** (admin shell):

```powershell
winget install usbipd
usbipd list                      # find the ST-Link BUSID, e.g. 2-3
usbipd bind --busid 2-3          # once per device
```

Every time the probe is (re)plugged:

```powershell
usbipd attach --wsl --busid 2-3   # add --auto-attach to survive replugs
```

The device then appears in WSL and (via the /dev/bus/usb mount +
privileged) inside the container:

```sh
lsusb                # should list "STMicroelectronics ST-LINK"
openocd -f interface/stlink.cfg -f target/stm32l4x.cfg
```

Flash:

```sh
openocd -f interface/stlink.cfg -f target/stm32l4x.cfg \
  -c "program flight-software/out/btc/btc/Debug/btc.elf verify reset exit"
```

## ST-Link, variant B: GDB server over TCP ("über Port")

No USB passthrough needed - run the server where the probe is plugged in
(Windows or WSL host):

```powershell
openocd -f interface/stlink.cfg -f target/stm32l4x.cfg -c "bindto 0.0.0.0"
# or: ST-LINK_gdbserver from STM32CubeCLT
```

From inside the container:

```sh
gdb-multiarch flight-software/out/btc/btc/Debug/btc.elf \
  -ex "target extended-remote host.docker.internal:3333"
```

Cortex-Debug launch config for this variant: `"servertype": "external",
"gdbTarget": "host.docker.internal:3333"`.
