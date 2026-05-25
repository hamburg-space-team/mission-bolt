# ICD-004: UART - EXP3 controller <=> LiFi transceivers + Wired Stack

## Document Information

| Field | Value |
|-------|-------|
| Document ID | ICD-004 |
| Version | 0.1 |
| Status | Draft |
| Date | 2026-05-25 |
| Owner | Software Lead |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-05-25 | Max | Initial draft from CDR section 4.5 |

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
- Baud rate **921 600** planned. TODO: confirm during integration.
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

## Sample Cycle

Driven by the LED charging cycle. TODO: cycle period and burst
length are tunable parameters, will be tightened during integration
tests.

```
+--- LED charging --+- LED off, sample burst -+--- LED charging ---+
|     ~1-3 s        |       ~100 ms            |     ~1-3 s          |
```

During charge: both transceivers idle, wireless stack in shutdown.
When EXP3 turns the LEDs off:

1. Wireless stack wakes up on loss of light, samples from its
   capacitor, emits frames over LiFi B until energy is gone.
2. EXP3 sends `START` over the cable, Wired Stack emits the same
   sample stream on cable + LiFi A.
3. EXP3 sends `STOP` to end the burst.

### Cable Control Bytes

| Byte | Direction | Meaning |
|---|---|---|
| `!` (0x21) | EXP3 -> Wired | START burst |
| `.` (0x2E) | EXP3 -> Wired | STOP burst |
| BEL (0x07) | Wired -> EXP3 | ALIVE heartbeat (1 Hz when idle) |

Sample frames are delimited by `<`/`>` so control bytes don't
collide.

### Cross-Check

Each Wired Stack sample appears on both transports. EXP3 compares
them and records the result as `valid_mask` bit 3
(`cable_check_ok`) in `PayloadExp3StackA`. This is the science
contribution of the cable path - per-sample ground truth for the
LiFi link.

## Timing

| Parameter | Target | Notes |
|---|---|---|
| LED-off -> first wireless sample | <= 50 ms | TODO confirm |
| START -> first wired sample | <= 10 ms | TODO confirm |
| Per-frame transmit time | ~5 ms @ 921k | rough |
| Max burst | ~100 ms | bounded by supercap |
| Cycle period | 1.0-3.1 s | runtime tunable via status payload |

## Error Handling

CRC fail -> frame dropped, next valid frame continues the burst.
`last_burst_sample_count` reports how many made it through.

Wireless stack silent >= 50 ms after LED-off -> `GAP_MARKER` with
`LIFI_TIMEOUT`. Cycle continues.

Wired stack silent >= 10 ms after START -> `GAP_MARKER` for this
cycle. Three consecutive misses -> stack flagged failed. Wireless
burst still attempted.

## Constants

| Name | Value |
|---|---|
| `UART_BAUD` | 921 600 (TODO confirm) |
| `SAMPLE_FRAME_SIZE` | 39 (TODO finalise) |
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
