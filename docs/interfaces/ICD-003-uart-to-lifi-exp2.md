# ICD-003: UART - EXP2 controller <=> LiFi transceivers

## Document Information

| Field | Value |
|-------|-------|
| Document ID | ICD-003 |
| Version | 0.1 |
| Status | Draft |
| Date | 2026-05-25 |
| Owner | Software Lead |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-05-25 | Max | Initial draft from CDR section 4.5 |

## Purpose

How EXP2 talks to its two STM32U031F8 LiFi transceivers over UART
to run a per-tick BER measurement.

The transceiver command protocol itself is shared with EXP3; see
[ICD-004](ICD-004-uart-to-lifi-exp3.md) for how EXP3 uses it
differently (feed-through, not buffer).

## Scope

In:

- UART parameters and command framing
- Buffer-mode (polling) sequence used by EXP2
- ACK/NAK and timeout behaviour
- Patterns and rate sweep (best current understanding)

Out of scope:

- Optical-layer behaviour between transceivers
- LiFi firmware state machine -- see CDR section 4.6
- BER payload on the downlink -- [ICD-007](ICD-007-packet-payloads.md)

## Parties

| Component | Hardware | Role |
|---|---|---|
| EXP2 Controller | STM32L476RGT6 | Drives both transceivers, computes BER per tick |
| LiFi TX | STM32U031F8 | Emits the test pattern |
| LiFi RX | STM32U031F8 | Captures the incoming pattern |

Each transceiver is on its own UART instance. The hardware is
symmetric; TX/RX role is software-configured at startup.

## Physical Layer

- UART, single-ended, 3.3 V CMOS, 8N1, no flow control.
- Baud rate: **921 600** planned. TODO: confirm during integration; fall
  back to 460 800 if we see errors at 921k.
- Molex connector on the EXP2 PCB also supplies 3.3 V to the
  transceiver.

## Command Protocol

Commands are prefixed by a backslash byte (`0x5C`). Most commands
ACK (`0x06`) or NAK (`0x15`); the read commands return data instead.

| # | Bytes | Meaning |
|---|---|---|
| 1 | `\N` + uint16 LE | Set buffer size (<= 10 KB) |
| 2 | `\s` + N bytes | Load buffer |
| 3 | `\S` | Send the buffer over LiFi |
| 4 | `\f` + uint32 LE | Set LiFi baud rate |
| 5 | `\R` | Enable RX |
| 6 | `\X` | Disable RX |
| 7 | `\F` | Switch to feed-through (used on EXP3) |
| 8 | `\P` | Switch to polling/buffer (EXP2 default) |
| 9 | `\C` | Clear buffer |
| 10 | `\?` -> uint16 LE | Number of received bytes |
| 11 | `\r` -> bytes | Drain RX buffer |

In feed-through mode (`\F`) a literal `0x5C` inside data is escaped
as `0x5C 0x5C`. EXP2 uses polling mode where the buffer length set
in command 1 is the length and no escaping is needed.

## Test Matrix

Rate table (indexed by `PayloadExp2Ber::rate_index`):

| Index | LiFi bit rate |
|---|---|
| 0 | 9 600 |
| 1 | 38 400 |
| 2 | 115 200 |
| 3 | 460 800 |
| 4 | 921 600 |

TODO: confirm values during integration. The downlink only carries
the index; ground has to know which rate-table revision was active.

Test patterns:

| Code | Pattern |
|---|---|
| `0x00` | PRBS-7 (default) |
| `0x01` | PRBS-15 |
| `0x02` | All 0xFF |
| `0x03` | Alternating 0xAA |

First byte of each RX frame is compared against the expected pattern
start and reported as `sync_check` so high BER is not confused with
frame-level desync.

## Timing

| Parameter | Value | Notes |
|---|---|---|
| Command response | <= 20 ms | Host gives up if no ACK/NAK |
| Round-trip in tick | <= ~30 ms | Has to fit before the 40 ms boundary |

## Error Handling

A single failed measurement just gets logged as
`measurement_valid = 0`. EXP2 keeps cycling.

After **5 consecutive invalid measurements** the transceiver is
treated as failed: EXP2 emits `GAP_MARKER` with reason
`SENSOR_FAILED` and stops issuing LiFi commands for the rest of
flight. Env data continues. TODO: confirm the 5-tick threshold
with bench tests.

If the transceiver doesn't ACK within 20 ms, the tick retries from
step 1 on the next cycle. Three consecutive retries hitting the
same step -> failed.

## Constants

| Name | Value |
|---|---|
| `UART_BAUD` | 921 600 (TODO confirm) |
| `CMD_TIMEOUT_MS` | 20 |
| `MAX_BUFFER_BYTES` | 10 240 |
| `DEFAULT_PATTERN_BYTES` | 64 (= 512 bits) |
| `RATE_TABLE_LEN` | 5 (TODO confirm) |
| `FAILURE_THRESHOLD` | 5 (TODO confirm) |

## Notes

UART receive on EXP2 is interrupt-driven into a ring buffer. The
application drains the ring inside the tick loop -- no blocking
HAL receive calls (I-2).

EXP2 logs the full sent and received bit streams to its microSD for
post-flight per-bit analysis. The downlink only carries the
aggregated statistics.