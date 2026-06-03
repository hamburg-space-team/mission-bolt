# Flight Software

Embedded code for the four STM32 controllers in THHOR-BOLT REXUS 37.

For the architectural overview, see [../architecture.md](../architecture.md)
and [ADR-002](../docs/decisions/ADR-002-class-hierarchy.md).

## What's Here

```
flight-software/
+-- btc/                  BTC application (master controller)
+-- exp1-space-disco/     EXP1 application
+-- exp2-bouncy-castle/   EXP2 application
+-- exp3-floaty-boi/      EXP3 application
+-- test-nucleo-h753zi/   Dev-board sandbox
+-- shared/               Code shared by all four binaries
|   +-- core/             FlightComputer, NodeComputer, ExpComputer, Platform, BootState
|   +-- comms/            Packet framework, CAN protocol, CRC framing
|   +-- bus/              I2C bus wrapper
|   +-- sensors/          MS5611, TMP117, ICM-42686/42688, AS7265X
|   +-- storage/          SdStore + LittleFS glue
|   +-- led/              LED drivers, status LEDs
|   +-- timing/           DWT helpers
|   +-- utils/            Crc16, DeviceBase
+-- lib/littlefs/         Vendored LittleFS, pinned
+-- tests/                Host-side Catch2 unit tests
+-- bolt.csolution.yml    CMSIS-Toolbox solution
+-- CMakeLists.txt        Host test build
```

Each board directory holds its `<board>.cproject.yml`, the CubeMX
output under `generated/`, and a small `app/` with the
controller-specific glue.

## Build

See [docs/guides/building-flight-software.md](../docs/guides/building-flight-software.md).

Quick:

```bash
cbuild bolt.csolution.yml --context btc.Debug+btc
```

## Test

Host-side Catch2 suite:

```bash
cmake -S . -B build/tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```

Details: [docs/guides/running-flight-unit-tests.md](../docs/guides/running-flight-unit-tests.md).

## Flash / Debug

[docs/guides/debugging-with-openocd.md](../docs/guides/debugging-with-openocd.md).

## API Documentation

Headers carry Doxygen annotations. Build:

```bash
doxygen Doxyfile
```

Output lands in `build/doxygen/html/`. Details:
[docs/guides/generating-doxygen.md](../docs/guides/generating-doxygen.md).

## Toolchain

- C++20 (`-std=gnu++20`), `arm-none-eabi-gcc` 13.3.1
- CMSIS-Toolbox 2.12.0
- STM32CubeMX for HAL and peripheral init
- LittleFS v2.11.3 for SD
- Catch2 v3.7.1 for host tests

Why these: [ADR-006](../docs/decisions/ADR-006-cmsis-toolbox.md).

## Architecture One-Liner

Four STM32s. Each runs a 40 ms tick loop, no RTOS. BTC is the
master and gateway; EXP1/2/3 are CAN slaves. Same source tree
compiles into four binaries through the class hierarchy.

## Standards

- [C++ standard](../docs/standards/coding/cpp.md)
- [System invariants](../docs/standards/system-invariants.md)
- [HIC++ deviations](../docs/standards/coding/cpp.md#documented-deviations)
