# ADR-007: IWDG refresh once per tick, 600 ms timeout

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

**Timeout: 600 ms.** Roughly 15 missed tick boundaries before the
IWDG fires. Wide enough to absorb the worst-case SD garbage-
collection stall of 300 ms (ADR-004) with substantial margin, so a
single GC pass cannot trigger a spurious reset. Narrow enough to
catch a genuine flight-loop hang well inside one science-relevant
time window.

## What we looked at

- **Refresh in multiple places** -- defeats the point.
- **Refresh in a timer ISR** -- same, but worse.
- **40 ms or 80 ms timeout** -- would fire on legitimate long sensor
  reads.
- **200 ms timeout** -- not enough headroom over the documented
  300 ms worst-case SD stall.
- **1 s timeout** -- that's a lot of dead time during science.
- **Windowed watchdog (WWDG)** -- shares fate with the main clock,
  can't catch faults that disable the clock tree.

## Consequences

Good:
- One line, one place, easy to verify in review.
- Any loop hang > 600 ms ends in a recovery path
  ([ADR-008](ADR-008-noinit-ram-recovery.md)) preserving the
  monotonic tick.
- 600 ms covers the worst-case SD GC stall
  ([ADR-004](ADR-004-storage-ringbuffer.md)) without false trips.

Bad:
- One stuck subsystem inside the tick body burns the whole tick.
  Per-call timeouts (I-2) keep that bounded.
- After a watchdog reset, in-flight ringbuffer entries that
  hadn't reached SD are lost. Reboot count is downlinked.

## Notes

LSI on STM32L4 is nominally 32 kHz +-15 %. Worst-case effective
timeout is around 510 ms; we size the tick budget against that.

`Platform::kick_wdg` is a function pointer so host tests can stub
it. The IWDG itself isn't verified host-side; that's a target test.

The next `BOOT` packet carries `BootReason`, which lets ground
distinguish `COLD_START` / `WATCHDOG` / `SOFT_RESET`.

## References

- [ADR-001 Tick Loop](ADR-001-tick-architecture.md)
- [ADR-004 Storage Ringbuffer](ADR-004-storage-ringbuffer.md)
- [ADR-008 .noinit Recovery](ADR-008-noinit-ram-recovery.md)
- [`shared/core/platform.hpp`](../../flight-software/shared/core/platform.hpp)
