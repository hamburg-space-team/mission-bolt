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

```
flight-software/tests/
+-- unit/             host-portable test cases
+-- integration/      reserved for target-level tests later
```

Right now there's just `test_crc16.cpp`. Coming
([per the CDR](../cdr/)):

- `test_packet_builder.cpp` -- header construction + CRC framing
- `test_boot_state.cpp` -- warm/cold boot state machine
- `test_can_protocol.cpp` -- fragmentation encode/decode

## Adding a Test

1. Drop `test_<name>.cpp` under `tests/unit/`.
2. Register it in [`tests/CMakeLists.txt`](../../flight-software/tests/CMakeLists.txt):

```cmake
add_executable(test_<name> unit/test_<name>.cpp)
target_include_directories(test_<name> PRIVATE
    ${CMAKE_SOURCE_DIR}/shared)
target_link_libraries(test_<name> PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(test_<name>)
```

3. Rebuild and rerun.

Minimal case:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "utils/crc16.hpp"

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

TODO: not wired up yet. Plan is gcov + gcovr via a `tests-cov`
build directory. Until then, focus on test depth, not coverage
numbers.

## Related

- [Building the flight software](building-flight-software.md)
- [Debugging with OpenOCD](debugging-with-openocd.md)
- [`tests/CMakeLists.txt`](../../flight-software/tests/CMakeLists.txt)
