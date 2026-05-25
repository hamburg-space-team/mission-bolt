# Debugging with OpenOCD

Flash and step through a built `.elf` on real hardware. Assumes
you already have an `.elf` from the [build guide](building-flight-software.md).

PyOCD + CMSIS-DAP path is at the bottom; some HIL setups use that.

## Prereqs

- ST-Link V2/V3 probe (the one bundled with a Nucleo works)
- OpenOCD 0.12.0
- `gdb-multiarch`
- Optional: VS Code with Cortex-Debug

```bash
openocd --version
gdb-multiarch --version
```

## Connect

```bash
# STM32L4 family (BTC, EXP1/2/3)
openocd -f interface/stlink.cfg -f target/stm32l4x.cfg

# STM32H7 (dev Nucleo only)
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg
```

OpenOCD prints something like `[stm32l4x.cpu] Cortex-M4 r0p1 ...`,
then exposes the GDB server on port 3333 and a Telnet shell on
4444. Leave it running.

## Flash

One-shot, separate terminal:

```bash
openocd -f interface/stlink.cfg -f target/stm32l4x.cfg \
        -c "program out/btc/btc/Debug/btc.elf verify reset exit"
```

Swap the `.elf` path for any other target. VS Code tasks like
**"Flash btc"** wrap this.

## Interactive Debug

With OpenOCD running:

```bash
gdb-multiarch out/btc/btc/Debug/btc.elf
```

```
(gdb) target extended-remote :3333
(gdb) monitor reset halt
(gdb) load
(gdb) tbreak app_main
(gdb) continue
```

Useful:

```
break flight_computer.cpp:42
continue
next / step
backtrace
print my_var
monitor reset halt
```

## VS Code

The launch configs in
[`flight-software/.vscode/launch.json`](../../flight-software/.vscode/launch.json)
cover **Debug btc / exp1 / exp2 / exp3** and the test Nucleo. Each
runs the matching `Build *.Debug` task, launches OpenOCD via
Cortex-Debug, and halts at `app_main`.

## Looking at `.noinit`

After a watchdog reset
([ADR-008](../decisions/ADR-008-noinit-ram-recovery.md)) the
persistent struct should survive.

1. Map file (`out/btc/btc/Debug/btc.map`) -> find the `.noinit`
   user section / linker symbol.
2. In GDB: `x/4xw 0x2003FFE0` (use the actual address).
3. `monitor reset halt`, inspect again. Contents survive
   watchdog/soft resets, clear on power-on.

## Reading the SD Log

Pull power, eject the card, drop it in a reader. The log file is
`/log.bin` on a LittleFS volume.

Use [`littlefs-python`](https://github.com/littlefs-project/littlefs):

```bash
pip install littlefs-python
```

Then load with the config from
[ADR-003](../decisions/ADR-003-littlefs-filesystem.md)
(`block_size = 512`, etc.).

## PyOCD + CMSIS-DAP

Some HIL workstations use a CMSIS-DAP probe instead of ST-Link.

```bash
pyocd list
pyocd erase --probe cmsisdap: --chip --cbuild-run <cbuild-run-file>
pyocd load  --probe cmsisdap: --cbuild-run <cbuild-run-file>
pyocd gdbserver --probe cmsisdap: --connect attach --persist \
                --reset-run --cbuild-run <cbuild-run-file>
```

VS Code launches **CMSIS_DAP@pyOCD (launch)** and **(attach)** do
this for you. The `--cbuild-run` file comes alongside the `.elf`
from `cbuild`.

## Common Issues

- **`LIBUSB_ERROR_ACCESS` on Linux** -- install the OpenOCD udev
  rules, add yourself to `plugdev`, re-plug.
- **Wrong target reported** -- wrong `target/*.cfg`.
- **`jtag status contains invalid mode value`** -- power cycle.
  ST-Links get stuck after aborted flashes.
- **Debug build but breakpoint never hits** -- code is unreachable;
  set an earlier breakpoint.
- **Watchdog fires immediately after `continue`** -- IWDG enabled
  in `on_init()` ([ADR-007](../decisions/ADR-007-iwdg-watchdog-strategy.md)).
  For debug work, halt before `on_init()` or build a temporary
  image with IWDG disabled.
