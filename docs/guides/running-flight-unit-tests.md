# Running the Flight Unit Tests

Host-side Catch2 suite. Runs on your laptop. No hardware needed.

Covers the host-portable subset: CRC, packet building, state
machines, protocol encoding/decoding.

## Prereqs

- CMake 3.22+
- A C++20 compiler (`g++` 11+, `clang++` 14+)
- `ctest` (ships with CMake)

Catch2 v3.7.1 is fetched on first configure via CMake.

## Build and Run

```bash
cd flight-software
cmake -S . -B build/tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```

VS Code task: **"Test: Build & Run"** wraps the same three
commands.

## Layout

Tests live next to the code they test, named `<unit>.test.cpp`:

```
shared/utils/crc16.hpp        the unit
shared/utils/crc16.test.cpp   its tests
```

`flight-software/CMakeLists.txt` globs `*.test.cpp` across `shared/` and the
four `*/app/` trees and builds one executable per file. The flight
builds cannot pick them up: the cprojects list their sources
explicitly, so a `.test.cpp` next to flight code is inert there.

Current suites: `crc16.test.cpp`, `errors.test.cpp` (error trace
chain), `wcet.test.cpp` (CYCCNT wrap arithmetic).

## Adding a Test

Drop `<unit>.test.cpp` next to the code it tests, then rebuild and
rerun. No CMake registration needed -- the glob re-runs on build
(`CONFIGURE_DEPENDS`).

Minimal case (`shared/utils/crc16.test.cpp`):

```cpp
#include "utils/crc16.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("CRC-16 known vector", "[crc16]") {
    constexpr uint8_t data[] = {'1','2','3','4','5'};
    REQUIRE(Crc::compute(data, sizeof(data)) == 0xC0BE);
}
```

## What to test here

Good fit:

- Pure logic (CRC, encode/decode, state machines)
- Layout assertions (`static_assert` on size and offsets)
- Edge cases (max-length payloads, wrap-around)

Not here:

- HAL behaviour / peripheral timing -- target tests
- I2C / UART / CAN transactions -- HIL
- Tick scheduling jitter -- real hardware

## CI

Same `ctest` invocation runs on every push (GitHub Actions,
[`.github/workflows/`](../../.github/workflows/)). Failing test
blocks the PR.

If a test passes locally but fails in CI (or vice versa), check
compiler versions match.

## Coverage

Wired up as the `coverage` stage of `scripts/verify.sh`: a separate
`build/coverage` tree (sanitizers off) runs the same suites under
gcov and prints per-file lines reached. The number is a gap gauge,
never a threshold -- it counts only code the tests link at all.
Focus on test depth, not coverage numbers.

## Related

- [Building the flight software](building-flight-software.md)
- [Debugging with OpenOCD](debugging-with-openocd.md)
- [`flight-software/CMakeLists.txt`](../../flight-software/CMakeLists.txt)
