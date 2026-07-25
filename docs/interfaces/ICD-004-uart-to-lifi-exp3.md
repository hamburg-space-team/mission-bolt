# ICD-004: UART - EXP3 controller <=> LiFi transceivers + Wired Stack

## Document Information

| Field | Value |
|-------|-------|
| Document ID | ICD-004 |
| Version | 0.2 |
| Status | Draft |
| Date | 2026-05-25 |
| Owner | Software Lead |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-05-25 | Max | Initial draft from CDR section 4.5 |
| 0.2 | 2026-07-25 | Max | Update based on CDR |

## Purpose

Three UART links on EXP3:

- LiFi A -- optical link to the Wired Stack
- LiFi B -- optical link to the Wireless Stack
- Cable UART -- wired channel to the Wired Stack (also carries 3.3 V)

Unlike EXP2 ([ICD-003](ICD-003-uart-to-lifi-exp2.md)), EXP3 runs
the transceivers in **feed-through** mode (`\F`). The stacks send
sample frames; EXP3 parses them as they arrive.

## Parties

| Component | Hardware | Role |
|---|---|---|
| EXP3 Controller | STM32L476RGT6 | Drives cycle, parses streams, builds packets |
| LiFi A transceiver | STM32U031F8 | Bridges UART <-> LiFi to/from Wired Stack |
| LiFi B transceiver | STM32U031F8 | Bridges UART <-> LiFi to/from Wireless Stack |
| Wired Stack | STM32U031F8 | Cable + LiFi-A sample source |
| Wireless Stack | STM32U031F8 | LiFi-B sample source, supercap-powered |

## Physical Layer

- UART, 3.3 V, 8N1, no flow control.
- Baud rate **921 600**. Same value as EXP2 LiFi; documented
  fallback at 460 800 if integration testing shows bit errors.
- Cable UART uses a Molex connector that also carries 3.3 V and
  ground to the Wired Stack.

## Configuration vs Data

The same `\`-prefixed commands as [ICD-003](ICD-003-uart-to-lifi-exp2.md)
configure the transceivers at startup (`\F` to enter feed-through,
`\f` for LiFi baud). Once configured, data flows transparently.

In feed-through, a literal `0x5C` in the payload is escaped as
`0x5C 0x5C` on the wire. The host parser unescapes before checking
CRC.

## Sample Frame (Stack -> EXP3)

Both stacks emit the same frame layout. One field interpretation
differs (capacitor voltage on the Wireless Stack only).

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | `start` = `0x3C` (`<`) |
| 1 | 1 | `sample_index` |
| 2 | 1 | `burst_index` |
| 3 | 4 | `timestamp_us` (stack-local us since LO) |
| 7 | 12 | `mag_xyz_raw` (3x int32 LE, MMC5983MA) |
| 19 | 6 | `accel_xyz_raw` (3x int16 LE, ICM-42688-P) |
| 25 | 6 | `gyro_xyz_raw` (3x int16 LE, ICM-42688-P) |
| 31 | 2 | `tmp_raw` (int16 LE, TMP117) |
| 33 | 2 | `cap_voltage_mv` (Wireless only; 0 on Wired) |
| 35 | 1 | `valid_mask` |
| 36 | 2 | CRC-16/CCITT-FALSE over bytes 1..35 |
| 38 | 1 | `end` = `0x3E` (`>`) |

Frame length: **39 bytes**. CRC starts after the start marker so the
marker is unambiguous on the wire.

TODO: finalise frame layout against the actual stack firmware. The
layout above is the planning target.

## Power and Data Path Independence

The charging LED (AL8862QSP-13 driver, 7x Cree XPEBBL 485 nm in series,
667 mA nominal) is **constant on** for the whole science timeline and holds
the Wireless Stack supercapacitor at its nominal voltage. LiFi-A and
LiFi-B carry data only and have no influence on the power side.

Both stacks are therefore symmetric data sources: whether a stack is
powered by cable or by light, the communication pattern seen by the EXP3
controller is the same.

> **Superseded.** Earlier revisions of this document described a
> time-multiplexed cycle in which the LEDs were switched off so the
> Wireless Stack could wake "on loss of light" and empty its capacitor
> into a ~100 ms burst, with `START`/`STOP` over the cable. That design
> is gone. Charging is continuous, the paths are independent, and the
> stacks are driven by sync rather than by burst commands.

## Stack Lifecycle

Each stack passes through three states (SED Fig. 4.40):

| State | Behaviour |
|---|---|
| `BOOT` | Init peripherals and sensors, run self-test, go to `WAITING FOR SYNC` |
| `WAITING FOR SYNC` | **No sample transmission.** Listen on the LiFi UART for the sync pattern. Stay here indefinitely. |
| `SAMPLING` | Reset internal timer to zero, sample at the configured rate, send each sample with timestamp, sequence number and CRC. Update the sync record on each new sync. |

Neither stack starts sampling on its own. The first sync pulse moves both
to `SAMPLING` at the same moment, and that instant is the reference time
for the science timeline.

## Synchronisation

EXP3 sends sync pulses over **both LiFi paths simultaneously** -- over
LiFi rather than the cable, so both stacks see the same transmission
latency. Residual offset is constant and corrected post-flight.

| Event | Timing |
|---|---|
| First sync | at LO; establishes t = 0 |
| Periodic sync | every 10 s (250 ticks at 25 Hz), for clock-drift compensation |
| Sample rate | working assumption ~50 Hz per stack; finalised during integration |

Each stack records the local arrival timestamp of every sync and includes
it in the next sample packet, so post-flight analysis can measure actual
drift and build a unified time reference.

## Sample Packet

Self-contained, so single-packet corruption stays single-packet:

- stack-internal timestamp at the moment of sampling
- sequence number, incrementing per sample
- most recent sync sequence number, and the stack timestamp when it arrived
- sensor readings: TMP117, ICM-42688-P, MMC5983MA
- CRC over the whole packet

CRC failure drops the packet; the gap appears as a missing sequence
number.

## Cable UART

The Molex cable to the Wired Stack carries power and a UART used **for
integration testing**. It gives ground an observation path to the Wired
Stack that does not depend on LiFi-A, which is what makes "LiFi-A failed"
distinguishable from "Wired Stack failed" on the bench.

The Wireless Stack has no equivalent. It is reachable only over LiFi-B,
so its diagnosis rests entirely on that link plus whether it draws
charging power at all.

## Error Handling

CRC fail -> packet dropped, visible as a missing sequence number.

A stack that stops producing samples is reported with a `GAP_MARKER`.
Thresholds and the failed-stack criterion are to be confirmed during
integration testing.

## Constants

| Name | Value |
|---|---|
| `UART_BAUD` | 921 600 |
| `SAMPLE_FRAME_SIZE` | 39 (TODO finalise against stack firmware) |
| `WIRELESS_TIMEOUT_MS` | 50 (TODO confirm) |
| `WIRED_START_TIMEOUT_MS` | 10 (TODO confirm) |
| `MAX_BURST_MS` | 120 |
| `MAX_SAMPLES_PER_BURST` | 16 (TODO size to capacitor) |

## Notes

Three independent RX ring buffers on EXP3, one per link. The tick
body drains all three. Sample frames that span tick boundaries get
emitted in the tick they complete.

`PayloadExp3Status::latency_b_estimated_us` is a rolling average of
wireless-stack wakeup-to-sample latency. The control loop uses it
to decide when to power the LEDs back on so the stack finishes its
current sample first. `wait_a_used_us` lets ground verify the
scheduler's actual delay.

## References

- [ICD-002 CAN Protocol](ICD-002-can-protocol.md)
- [ICD-003 EXP2 LiFi](ICD-003-uart-to-lifi-exp2.md)
- [ICD-007 Packet Payloads](ICD-007-packet-payloads.md)
- [ADR-005 Fault Management](../decisions/ADR-005-fault-management.md)
- [`exp3-floaty-boi/app/`](../../flight-software/exp3-floaty-boi/app/)