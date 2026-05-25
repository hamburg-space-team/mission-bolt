# ADR-003: Producer-Consumer Ringbuffer for SD Logging

## Status

Accepted

## Date

2026-05-13

## Context

Each STM32L476 flight controller and the STM32F756 BTC log telemetry data 
to a microSD card via SDMMC and LittleFS. Consumer-grade SD cards stall 
unpredictably for 50-300 ms during internal flash management operations 
(garbage collection, wear levelling).

A synchronous `lfs_file_write()` call inside the 40 ms tick would cause 
the tick to overrun whenever such a stall occurs, violating Invariant I-1 
(deterministic tick budget) and risking a watchdog reset during a 
scientifically critical phase.

The flight software runs without an RTOS to maintain analysability by 
inspection (Principle P-3). Industrial-grade SD cards with bounded stall 
times exist but are cost-prohibitive for a student project.

## Decision

We decouple SD writes from the tick loop through a producer-consumer 
ringbuffer in RAM:

1. `on_tick()` writes log entries into the ringbuffer in microseconds and 
   returns immediately
2. The main loop drains the ringbuffer via `lfs_file_write()` calls 
   during the idle phase of each tick
3. A new write only starts if remaining tick time exceeds 
   `MIN_TIME_FOR_WRITE_MS` (5 ms)
4. The ringbuffer is statically sized for 64 entries to absorb stalls 
   up to 300 ms with margin

LittleFS itself caches up to 512 bytes per file. We explicitly call 
`lfs_file_sync()` every 25 ticks (1 Hz) and on critical events (lift-off, 
permanent sensor failures, BOOT packet, before planned soft resets).

## Alternatives Considered

### Alternative A: Synchronous Write Per Tick

Write directly from `on_tick()` without buffering.

**Rejected because:** Violates I-1 during SD garbage collection stalls. 
A single stall can extend a tick to 300+ ms, well beyond the 40 ms 
budget. This would either cause systematic missed ticks or trigger the 
watchdog.

### Alternative B: RTOS with SD Task on Lower Priority

Move SD write logic to a FreeRTOS task with priority below the tick loop.

**Rejected for now:** Adds complexity (FreeRTOS configuration, task 
synchronisation, stack sizing) that we want to defer. The producer-
consumer pattern provides equivalent decoupling without the RTOS overhead. 
Documented as contingency: if integration testing shows the missed-tick 
rate exceeds experiment tolerance, we migrate the drain logic to an 
RTOS task. The ringbuffer and LittleFS interface remain unchanged; only 
the consumer's execution context changes.

### Alternative C: SPI Flash Chip Instead of SD Card

Replace SD card with a dedicated SPI flash chip with bounded write times.

**Rejected because:** Requires PCB redesign which is not feasible at 
this project stage. Would also lose the convenience of removable storage 
for ground testing.

### Alternative D: Custom Append-Only Filesystem

Build a custom filesystem optimised for our access patterns.

**Rejected because:** Significant development effort to verify power-loss 
safety properties that LittleFS already provides. Verification cost is 
not justified for a single-mission deployment.

## Consequences

### Positive

- `on_tick()` execution time becomes deterministic, maintaining I-1
- LittleFS power-loss safety is preserved per I-7
- No RTOS complexity required, maintaining the analysability-by-
  inspection property per P-3
- Migration path to RTOS preserved if needed; only consumer context 
  changes
- Code structure is the same on all four flight controllers

### Negative

- Long SD stalls (>15 ms) still cause individual missed ticks during 
  the drain phase
- Worst-case 1 second data loss on mid-tick reboot due to 25-tick sync 
  interval
- Increased RAM usage: 64 entries x 60 bytes ~ 4 KB per controller

### Neutral

- Code structure requires explicit producer/consumer separation
- Status payloads must include `sd_dropped_count` field for monitoring 
  ringbuffer overflow events

## Implementation Notes

The ringbuffer uses memory barrier macros that compile to no-ops in the 
current no-RTOS configuration but become real barriers if migrated to 
FreeRTOS. This makes the eventual RTOS migration mechanical rather than 
a redesign.

Watchdog refresh happens before the SD drain phase, not after, so a 
stuck drain cannot prevent the watchdog from triggering recovery.

The sync interval of 25 ticks (1 Hz) was chosen over per-tick sync to 
avoid forcing partial-block writes that undermine LittleFS's natural 
block-aligned commit cycle. Per-tick sync would increase SD card wear 
without meaningful data-loss reduction.

## References

- Related ADRs: [ADR-001 Tick Architecture](ADR-001-tick-architecture.md)
- Related ICDs: [ICD-001 RS-422 to RXSM](../interfaces/ICD-001-rs422-to-rxsm.md)
- Implementation: [`flight-software/src/shared/storage/`](../../flight-software/src/shared/storage/)
- System Invariants: [I-1 and I-7](../architecture/system-invariants.md)

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-05-15 | @maxatslega | Initial draft based on storage discussion |
| 1.0 | 2026-05-15 | @maxatslega | Approved |