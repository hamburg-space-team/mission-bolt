# C++ Coding Standard (Flight Code)

The flight code is C++23 (`-std=gnu++23`). We picked C++23
specifically for `std::expected`, which is our primary
error-handling mechanism (see Error handling below). The standard
we follow in spirit is HIC++ (High Integrity C++). We don't claim
full compliance, we claim the parts that matter for a single-shot
embedded payload, and we document the carve-outs. ECSS-E-ST-40C is
used as a guide for our review phases; we do not target full
compliance with the heavyweight aerospace standards.

## Hard Rules

These come from the system invariants
([`system-invariants.md`](../system-invariants.md)). Breaking them
needs an ADR.

- **No dynamic allocation after `on_init()`** (I-3). No `new`,
  `malloc`, `std::vector`, `std::string`. Static arrays, fixed
  buffers.
- **Every I/O call has a hard timeout** (I-2). No unbounded loops
  on the critical path.
- **No silent fallback values** (I-6). When a sensor read fails,
  clear its bit in `valid_mask`. Don't substitute.
- **No exceptions.** `-fno-exceptions` in the flight build.
- **No RTTI.** `-fno-rtti`.
- **Watchdog refresh in exactly one place** ([ADR-007](../../decisions/ADR-007-iwdg-watchdog-strategy.md)).

## Style

Enforced by `clang-format` (config in [`.clang-format`](../../../flight-software/.clang-format))
and `clang-tidy` ([`.clang-tidy`](../../../flight-software/.clang-tidy)).

- Indentation, braces, line length -> formatter handles it. Don't
  argue about it.
- Naming: `lower_case` for variables and member functions,
  `UPPER_CASE` for constants, `CamelCase` for types.
- Shadowing a parameter -> use `this->` explicitly. clang-tidy
  auto-fixes.
- Mark functions `[[nodiscard]]` when ignoring the return is a
  bug.
- `noexcept` where it actually means something.
- `final` on leaf classes.

## clang-tidy

We enable in full:

- `readability-*`
- `modernize-*`
- `performance-*`
- `bugprone-*`
- `clang-analyzer-*`
- `cppcoreguidelines-*`

The host-portable subset is checked from the command line. Board-
specific code that depends on the STM32 HAL is checked in-editor
through clangd, which understands ARM builtins.

### Documented Deviations

| ID | Rule | Why |
|---|---|---|
| DEV-SW-001 | `__attribute__((packed))` | GCC extension for binary protocol serialisation |
| DEV-SW-002 | Inline asm in HAL/CMSIS | Unavoidable in vendor code (Cortex-M intrinsics, NVIC) |
| DEV-SW-003 | C-style arrays in packed structs | `std::array` isn't layout-compatible with byte-exact protocols |
| DEV-SW-004 | `hicpp-signed-bitwise` in MS5611 driver | Datasheet algorithm prescribes signed arithmetic |

Anything else needs justification in the code (single-line `NOLINT`
with an explanation) or, for systemic carve-outs, an ADR.

## Patterns We Use

### Sensor drivers

Inherit from [`DeviceBase`](../../../flight-software/shared/utils/device_base.hpp).
On every transaction, call `register_failure()` on error or
`clear_failures()` on success. Three strikes and the device is
out. [ADR-005](../../decisions/ADR-005-fault-management.md).

### Packet building

Use [`PacketBuilder`](../../../flight-software/shared/comms/packet_builder.hpp).
Write directly into a caller-owned buffer. No allocation.

### Static buffers in classes

Sized at compile time. Aligned where the underlying API needs it
(LittleFS, DMA). Lives for the lifetime of the program.

## What We Don't Use

- Exceptions, RTTI, the C++ standard library beyond `<cstdint>`,
  `<cstddef>`, `<array>`, `<expected>`.
- `std::function`, `std::shared_ptr`, anything that hides
  allocation.
- Floating-point on the critical path unless the sensor demands
  it. Raw integer registers go to ground; conversion happens there
  ([ADR-009](../../decisions/ADR-009-raw-sensor-data-to-ground.md)).

## Error handling

`std::expected<T, E>` is the primary error-handling mechanism.
Functions that can fail return an expected; the caller has to
handle both the value and the error branch before using the
result. Three reasons:

- Exceptions are off (`-fno-exceptions`), so they're not available
  anyway.
- The error path is visible in the signature, not hidden in an
  out-parameter or a sentinel return.
- The compiler enforces that the caller handle the error before
  unwrapping. No "I forgot to check the return value" bugs.

```cpp
[[nodiscard]] std::expected<int16_t, I2cError> read_temperature();
```

Sentinel returns (`int read(...)` with `0 = OK`, non-zero = error)
are still used in the existing `DeviceBase` API and in HAL
wrappers. New code should prefer `std::expected` where it doesn't
fight the surrounding code.

## Comments

Two layers: Doxygen on the public API in headers, inline comments
only where the *why* is non-obvious.

### Doxygen -- public API only

Use `///` line comments. The first line is the brief.

Write Doxygen for:

- Public classes, structs and free functions
- Public/protected methods *with non-obvious semantics* (timeouts,
  error conditions, side effects, ordering constraints, units)

Skip Doxygen for:

- Trivial getters/setters -- the name says everything
- Private members and methods -- implementation detail
- `.cpp` files -- the matching header carries the API
- Generated code (CubeMX, HAL)

```cpp
/// Reads the raw temperature register.
///
/// @param raw Destination; must not be nullptr.
/// @return 0 on success; failures latch via DeviceBase.
[[nodiscard]] int read(int16_t* raw);
```

If a better name removes the need for the comment, rename. A
specific name beats a documented generic one
(`read_temperature_centiCelsius` over `read` + Doxygen).

### Inline comments -- only when the *why* is non-obvious

Default to none. Identifiers should carry the meaning. Add a
comment only when the *why* is non-obvious: a hidden constraint,
a workaround for a specific datasheet quirk, a citation to an
invariant or ADR.

Bad:

```cpp
// Increment counter
counter++;
```

OK:

```cpp
// refresh once per tick. Refreshing elsewhere would mask
// stalls in specific subsystems.
platform.kick_wdg();
```

### Cross-references

When code implements a requirement, invariant or decision, name
it explicitly so future readers can navigate back to the context:

```cpp
/// Reads the 18-channel spectrum (per F-10).
Result read_channels(uint16_t* values, uint32_t timeout_ms);

// I-2: hard timeout on every I2C read.
i2c.read(addr, reg, &value, timeout_ms);

// See ADR-007 for the IWDG strategy.
platform.kick_wdg();
```

### Honesty

If a comment becomes false, fix it or delete it. Update the
comment in the same commit as the code change. Never document
behaviour you have not verified.

## Tests

- New algorithmic logic gets a host-side Catch2 test.
- Bug fixes get a regression test.
- Target-specific behaviour gets a HIL test, not a host test.

See [running-flight-unit-tests](../../guides/running-flight-unit-tests.md).

## Reviewing

Look for:

- **Invariants.** Cite by number. "Violates I-2, add a deadline" is
  more useful than "this seems risky".
- **Allocation.** Anything that smells like `new` or heap-bound
  containers.
- **Error paths.** Every return value checked. Every failure
  surfaced.
- **Comments.** Doxygen blocks on public APIs -- required. Inline
  body comments -- only if they explain *why*. Delete the "*what*"
  ones.

## References

- [System Invariants](../system-invariants.md)
- HIC++ -- High Integrity C++ Coding Standard (in our shared drive)
- [`.clang-tidy`](../../../flight-software/.clang-tidy)
- [`.clang-format`](../../../flight-software/.clang-format)
