# ADR-001: Deterministic 40 ms Tick Loop Without RTOS

## Status

Accepted

## Date

2026-05-13

## Context

THHOR-BOLT runs on four STM32 controllers (BTC + EXP1-3) that must
sample sensors, drive experiment hardware, exchange data over CAN, and
emit downlink packets, all during a single ~600 second flight window.
We cannot push fixes once airborne. Whatever timing model we choose has
to be analysable by inspection and survive a code review by people who
were not in the room when the decision was made.

The classical embedded options are: a preemptive RTOS (FreeRTOS, OS2),
a cooperative scheduler, or a fixed-period super-loop. Each makes
different trade-offs between flexibility and analysability.

The system invariants drive the choice:

- I-1 requires a bounded execution time per tick.
- I-2 requires bounded blocking on every I/O call.
- I-3 forbids dynamic allocation after init.
- P-3 prefers predictability over performance.

## Decision

Every flight controller runs a single execution context that ticks at a
fixed period of **40 ms** (25 Hz). The tick body performs sensor reads,
packet construction, CAN/UART/RS-422 transmission, an SD enqueue, and
exactly one IWDG refresh. After the tick body, the loop spins on a
monotonic time reference until the next 40 ms boundary, draining the
SD ringbuffer opportunistically during the idle phase.

The tick loop is implemented in [`FlightComputer::run()`](../../flight-software/shared/core/flight_computer.hpp)
with `LOOP_PERIOD_MS = 40`. Two pure-virtual hooks define each
controller's behaviour: `on_init()` runs once before the flight loop;
`on_tick(tick_start_us, missed_periods)` runs every period.

If a tick overruns, the loop detects the overshoot, computes the number
of missed 40 ms boundaries, and reports them through the
`missed_periods` argument so they can be downlinked. The loop then
realigns to the next boundary; the science timeline does not drift.

## Alternatives Considered

### Alternative A: FreeRTOS or CMSIS-OS2

Run sensor sampling, downlink, and SD writes as separate priority-based
tasks.

**Rejected because:** Adds scheduler decisions, priority inversion
possibilities, and stack-sizing arguments that we cannot fully analyse
with the verification time we have. We would also have to justify the
RTOS configuration against I-3 (no dynamic allocation) and I-1 (bounded
execution time), which is harder than justifying a single-threaded
loop. Kept as a contingency for the SD drain path only; see
[ADR-004](ADR-004-storage-ringbuffer.md).

### Alternative B: Cooperative Scheduler with Coroutines

A run-to-completion scheduler that dispatches small tasks based on
elapsed time or events.

**Rejected because:** Still requires per-task time budgeting to satisfy
I-1, and adds an abstraction (task IDs, dispatch tables) that obscures
the actual control flow. We get the same predictability benefits with
the linear tick body and lose the scheduler complexity.

### Alternative C: Different Tick Period (10 ms, 100 ms)

Faster tick (10 ms) for finer time resolution, or slower (100 ms) for
more margin.

**Rejected because:** 40 ms is the cadence at which the EXP2 BER
measurement, the EXP1 spectrometer integration windows, and the
RS-422 downlink rate align cleanly. A 10 ms tick would not buy
anything scientifically and would tighten the budget below what the
SD subsystem can tolerate. 100 ms would not give the spectrometer
enough sample cycles per flight.

## Consequences

### Positive

- Tick body is straight-line code: read sensors, build packet, send,
  log, kick watchdog. No scheduler state to reason about.
- I-1 is enforced structurally: every operation lives inside the
  same 40 ms window.
- The four flight controllers share the same loop, which keeps the
  class hierarchy uniform (see [ADR-002](ADR-002-class-hierarchy.md)).
- The IWDG strategy collapses to "refresh once per tick"; we never
  refresh from multiple sites (see [ADR-007](ADR-007-iwdg-watchdog-strategy.md)).
- Missed-period accounting is local and explicit: the loop knows how
  many boundaries it skipped and reports them.

### Negative

- A single misbehaving operation in the tick body affects everything
  else. We mitigate this with hard timeouts on every I/O path (I-2)
  and the ringbuffered SD drain ([ADR-004](ADR-004-storage-ringbuffer.md)).
- No background work below the loop. The SD drain is the only thing
  we run during the idle phase, and it is throttled by
  `MIN_TIME_FOR_WRITE_MS = 5 ms`.

### Neutral

- The same code pattern repeats on all four boards. Reviewers learn
  the shape once.
- Time is measured in tick numbers, not wall clock. Downlink and SD
  consumers correlate by tick.

## Implementation Notes

> Todo

## References

- Related ADRs: [ADR-002](ADR-002-class-hierarchy.md),
  [ADR-004](ADR-004-storage-ringbuffer.md),
  [ADR-007](ADR-007-iwdg-watchdog-strategy.md)
- System Invariants: [I-1, I-2, I-3](../standards/system-invariants.md)
- Implementation: [`flight-software/shared/core/flight_computer.hpp`](../../flight-software/shared/core/flight_computer.hpp)

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-05-15 | @maxatslega | Initial draft |
| 1.0 | 2026-05-15 | @maxatslega | Approved |
