# C++ Coding Standard (Flight Code)

The flight code is C++20 (`-std=gnu++20`). The standard we follow
in spirit is HIC++ (High Integrity C++). We don't claim full
compliance, we claim the parts that matter for a single-shot
embedded payload, and we document the carve-outs.

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
  `<cstddef>`, `<array>`.
- `std::function`, `std::shared_ptr`, anything that hides
  allocation.
- Floating-point on the critical path unless the sensor demands
  it. Raw integer registers go to ground; conversion happens there
  ([ADR-009](../../decisions/ADR-009-raw-sensor-data-to-ground.md)).

## Comments

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
- **Comments.** Are they explaining *why*? If they explain *what*,
  delete them.

## References

- [System Invariants](../system-invariants.md)
- HIC++ -- High Integrity C++ Coding Standard (in our shared drive)
- [`.clang-tidy`](../../../flight-software/.clang-tidy)
- [`.clang-format`](../../../flight-software/.clang-format)
