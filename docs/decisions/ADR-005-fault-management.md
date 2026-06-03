# ADR-005: Three-strike sensor failure + explicit gap markers

## Status

Accepted

## Date

2026-05-13

## Context

P-1: things will go wrong. The interesting question is what survives
when a sensor hangs, a CAN node drops, or an SD write fails.

A single bad I2C read is bus noise, not a broken sensor. But a sensor
that's actually dead burns tick budget every cycle. We need both
ends of that to be visible.

Per I-6 the ground side must always know whether a missing slot is
"no data" or "we lost the downlink."

## Decision

**Three strikes.** Every sensor is a `DeviceBase`. After
`MAX_FAILURES = 3` consecutive errors the sensor is disabled -- no
more reads, `valid_mask` bit cleared in the next payload, latched
for the rest of the flight. A success at any point before strike 3
clears the counter.

Three was chosen so bus noise doesn't retire a healthy sensor but a
real fault is caught within 120 ms.

**GAP_MARKER packets.** The BTC emits a `GAP_MARKER` with a
`GapReason` whenever it can't include a source in a downlink slot:

- `NO_DATA` -- no valid CAN data was received from the source
  within the expected window
- `CAN_CRC_FAIL` -- the CAN frame was received but failed CRC
- `LIFI_TIMEOUT` -- no sample packets received from an EXP3 stack
  within the expected interval (degraded or lost LiFi link)
- `SENSOR_FAILED` -- the source has marked a sensor as permanently
  disabled

Defined in [`packet_types.hpp`](../../flight-software/shared/comms/packet_types.hpp).

**AS7265X recovery.** Spectrometer gets one reset attempt before
the three-strike rule kicks in: hold reset, wait three ticks
(120 ms), release, re-init.

**Autonomous mode.** Experiment controllers that miss 5 consecutive
SYNCs (200 ms) generate their own local tick, write SD only, and
re-sync the moment SYNC returns. State is flagged in the next status
payload so ground can stitch SD against the recovered downlink.

## What we looked at

- **Retire on first error** -- too aggressive, kills healthy sensors
  to bus noise.
- **Retry forever** -- violates I-2, eats budget every tick.
- **Implicit gaps** -- ground can't tell sensor failure from
  downlink loss. Violates I-6.

## Consequences

Good:
- Transient blips don't lose sensors.
- Permanent failures stop costing tick budget.
- Ground timeline is always honest: real data, masked bit, or
  GAP_MARKER with a reason.

Bad:
- A burst of three real timeouts disables a sensor that might
  recover later. No re-enable path in flight.

## Notes

The 200 ms autonomous-mode threshold balances false positives
(single missed SYNC from bus noise) against responsiveness. We
will measure the actual false-positive rate during integration
tests; if needed it can be retuned in
[ICD-002](../interfaces/ICD-002-can-protocol.md).

EXP controllers report sensor failures over CAN via `valid_mask`.
The BTC promotes those into `GAP_MARKER` if it has to drop the slot.

## References

- [ADR-001 Tick Loop](ADR-001-tick-architecture.md)
- [ADR-007 IWDG](ADR-007-iwdg-watchdog-strategy.md)
- [ADR-008 .noinit Recovery](ADR-008-noinit-ram-recovery.md)
- [`shared/utils/device_base.hpp`](../../flight-software/shared/utils/device_base.hpp)
