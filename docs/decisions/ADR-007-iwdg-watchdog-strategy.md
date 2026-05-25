# ADR-007: IWDG refresh once per tick, 200 ms timeout

## Status

Accepted

## Date

2026-05-13

## Context

The IWDG is the only thing that can save us from a tick-loop hang
because it runs off the independent LSI clock. Two design choices:
where in code do we refresh, and what timeout do we configure.

Mess this up and the watchdog is useless: refreshes from interrupts
or from multiple sites can keep the dog alive while the actual
flight loop is stuck.

## Decision

**One refresh site.** `platform.kick_wdg()` is called exactly once
per tick, at the end of the tick body in `on_tick()`. Never from
interrupts, never from drivers, never from anywhere else.

**Timeout: 200 ms.** Five missed tick boundaries before the IWDG
fires. Wide enough to absorb a single long sensor read or a long
SD drain stall, narrow enough to be useful in a 600 s flight.

## What we looked at

- **Refresh in multiple places** -- defeats the point.
- **Refresh in a timer ISR** -- same, but worse.
- **40 ms or 80 ms timeout** -- would fire on legitimate long sensor
  reads.
- **1 s timeout** -- that's a lot of dead time during science.
- **Windowed watchdog (WWDG)** -- shares fate with the main clock,
  can't catch faults that disable the clock tree.

## Consequences

Good:
- One line, one place, easy to verify in review.
- Any loop hang > 200 ms ends in a recovery path
  ([ADR-008](ADR-008-noinit-ram-recovery.md)) preserving the
  monotonic tick.
- 200 ms covers the worst-case SD GC stall
  ([ADR-004](ADR-004-storage-ringbuffer.md)) without false trips.

Bad:
- One stuck subsystem inside the tick body burns the whole tick.
  Per-call timeouts (I-2) keep that bounded.
- After a watchdog reset, in-flight ringbuffer entries that
  hadn't reached SD are lost. Reboot count is downlinked.

## Notes

LSI on STM32L4 is nominally 32 kHz +-15 %. Worst-case timeout is
~170 ms; we size the tick budget against that.

`Platform::kick_wdg` is a function pointer so host tests can stub
it. The IWDG itself isn't verified host-side; that's a target test.

The next `BOOT` packet carries `BootReason`, which lets ground
distinguish `COLD_START` / `WATCHDOG` / `SOFT_RESET`.

## References

- [ADR-001 Tick Loop](ADR-001-tick-architecture.md)
- [ADR-004 Storage Ringbuffer](ADR-004-storage-ringbuffer.md)
- [ADR-008 .noinit Recovery](ADR-008-noinit-ram-recovery.md)
- [`shared/core/platform.hpp`](../../flight-software/shared/core/platform.hpp)
