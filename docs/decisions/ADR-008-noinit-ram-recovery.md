# ADR-008: Mid-flight recovery via `.noinit` SRAM

## Status

Accepted

## Date

2026-05-13

## Context

A watchdog reset during flight cannot look like a fresh boot:

- The tick counter restarts -> breaks I-4.
- Ground can't correlate post-reset data with pre-reset data.

REXUS keeps the experiment powered the whole flight, so the only
resets we expect are software-triggered. On STM32, software resets
preserve SRAM. A small uninitialised SRAM region therefore survives.

## Decision

Persistent struct in a `.noinit` linker section. Fields:

- `magic` -- `0xB017BEEF`, validates the rest
- `tick` -- saved periodically by the BTC
- `reboot_count` -- total non-cold-start resets since first power-on

`BootState::read()` runs on every boot:

1. Read + clear RCC reset flags.
2. Classify the boot: `COLD_START` / `WATCHDOG` / `SOFT_RESET`.
3. If magic matches and reason != cold start -> warm recovery, bump
   `reboot_count`, return `tick_valid = true` on the BTC.
4. Otherwise -> clear the struct, cold boot path.

The BTC calls `BootState::save_tick()` every 25 ticks. Experiment
controllers don't persist their own tick; after reset they wait
for the next CAN SYNC (<= 40 ms) and adopt the BTC value.

Every controller emits a `BOOT` packet at the top of `on_init()`
with reason + reboot count + (BTC) recovered tick.

## What we looked at

- **Persist to SD** -- write latency unbounded, can't go on the
  critical path.
- **Persist to on-chip flash** -- wear, latency. Wrong tool here.

## Consequences

Good:
- Watchdog/soft resets don't break the science timeline.
- Ground can tell warm recovery from cold start via the BOOT
  payload (I-6, P-4).
- Reboot count surfaces controllers that reset repeatedly.
- Cheap: ~16 B `.noinit` + one write per second on the BTC.

Bad:
- Power loss (brown-out, harness fault) loses the persistent
  state. Cold boot. Acceptable: REXUS power should be solid.
- Recovered tick can lag the actual reset by up to 24 ticks
  (~960 ms). EXPs re-sync immediately on the next SYNC, so the
  error doesn't propagate.

## Notes

`.noinit` section is added through a CubeMX user section in the
linker script. Don't hand-edit the rest of the generated linker
file.

Magic word is sanity, not security. It distinguishes valid state
from random SRAM at first boot.

## References

- [ADR-001 Tick Loop](ADR-001-tick-architecture.md)
- [ADR-005 Fault Management](ADR-005-fault-management.md)
- [ADR-007 IWDG](ADR-007-iwdg-watchdog-strategy.md)
- [`shared/core/boot_state.hpp`](../../flight-software/shared/core/boot_state.hpp)
