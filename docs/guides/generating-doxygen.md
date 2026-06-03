# Generating the Flight-Software Doxygen

The flight-software headers carry Doxygen annotations on the
public API. This guide explains how to render them.

## Prerequisites

```bash
sudo apt install doxygen graphviz
```

Graphviz is optional but gives you the inheritance/include/call
graphs.

## Build

From `flight-software/`:

```bash
doxygen Doxyfile
```

Output appears in `flight-software/build/doxygen/html/`. Open
`build/doxygen/html/index.html` in a browser.

Warnings are written to `build/doxygen/warnings.log`. Each PR that
touches a header should keep the log empty.

### Via CMake

The host-test CMake project exposes a `docs` target when Doxygen
is on `$PATH`:

```bash
cmake -S . -B build/tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests --target docs
```

## Conventions

- `///` line comments, first line is the brief.
- Doxygen on public classes, structs, free functions, and
  public/protected methods with non-obvious semantics. Skip trivial
  getters, private members, and `.cpp` files. See
  [cpp.md](../standards/coding/cpp.md#comments) for the rules.
- Each module belongs to one `@defgroup`:
  - `utils` - `device_base`, `crc16`
  - `bus` - I2C wrapper
  - `timing` - DWT helpers
  - `comms` - packet framework, CAN protocol/transport
  - `sensors` - IMU, baro, temperature, spectrometer drivers
  - `led` - single-LED abstraction, status LEDs, LP5810
  - `storage` - `SdStore` + LittleFS glue
  - `core` - `FlightComputer`/`NodeComputer`/`ExpComputer`, boot
    recovery, platform shim
  - `apps` - BTC/EXP1/EXP2/EXP3 controllers and their `main.cpp`
- Cross-reference with `@ref <Class>` or `@ref <function>`.
