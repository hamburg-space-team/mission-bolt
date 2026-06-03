# ICD-001: RS-422 Link Between BTC and REXUS Service Module

## Document Information

| Field | Value |
|-------|-------|
| Document ID | ICD-001 |
| Version | 1.0 |
| Status | Approved |
| Date | 2026-05-25 |
| Owner | Software Lead |
| Reviewers | Hardware Lead, Communications Lead |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-05-15 | Max | Initial draft from CDR section 4.3 |
| 1.0 | 2026-05-25 | Max | Approved for CDR |

## Purpose

Defines the external telemetry interface between the Bifröst Test
Controller (BTC) and the REXUS Service Module (RXSM). The BTC is the
only on-board node with a direct interface to the rocket: this link
carries the aggregated downlink stream from all four flight controllers
to the ground station via the REXUS PCM telemetry chain.

The same connector also carries the three discrete REXUS signals
(LO, SODS, SOE) used as the science timeline reference, and the
+28 V experiment power.

## Scope

This document specifies:

- The RS-422 physical layer between BTC and RXSM
- The REXUS connector pinout used by THHOR-BOLT
- The discrete LO, SODS, and SOE signal handling
- Downlink framing on the byte-stream link
- Bandwidth budget and error handling

This document does not cover:

- The downlink packet structure inside the frame (see
  [ICD-006](ICD-006-downlink-packet-format.md))
- The PCM/RF chain between RXSM and ground (handled by REXUS;
  see REXUS PCM Telemetry System RX00-101006)
- Power distribution downstream of the BTC fuse

## Parties

| Component | Hardware | Role |
|-----------|----------|------|
| BTC | STM32L476RGT6 | Transmits downlink; samples LO/SODS/SOE; receives uplink (reserved) |
| RXSM | REXUS Service Module | Forwards downlink to ground via PCM; sources LO/SODS/SOE |

## Physical Layer

### Electrical Specification

| Parameter | Value | Notes |
|-----------|-------|-------|
| Standard | TIA/EIA-422-B | Differential, point-to-point |
| Bit rate | 115 200 baud | Default |
| Framing | 8N1 | 8 data bits, no parity, 1 stop |
| Flow control | None | |
| Direction | Differential pair per direction | |

Downlink uses **EXP out+/EXP out-** (RXSM pins 6/7). The uplink
pair **EXP in+/EXP in-** (pins 13/14) is wired but not currently
used; the BTC ignores received bytes.

### REXUS Connector and Pinout

| Pin | Name | Direction | Use |
|-----|------|-----------|-----|
| 1 | +28 V | RXSM -> BTC | Experiment power |
| 2 | Charging (28 V/1 A) | -- | Not connected |
| 3 | SODS | RXSM -> BTC | Start of Data Storage (discrete) |
| 4 | SOE | RXSM -> BTC | Start of Experiment (discrete) |
| 5 | LO | RXSM -> BTC | Lift-Off (discrete) |
| 6 | EXP out+ | BTC -> RXSM | Downlink, non-inverted |
| 7 | EXP out- | BTC -> RXSM | Downlink, inverted |
| 8 | 28 V Ground | -- | Ground |
| 9 | +28 V | RXSM -> BTC | Experiment power |
| 10 | n.c. | -- | Not connected |
| 11 | n.c. | -- | Not connected |
| 12 | Charging Return | -- | Not connected |
| 13 | EXP in+ | RXSM -> BTC | Uplink, non-inverted (reserved) |
| 14 | EXP in- | RXSM -> BTC | Uplink, inverted (reserved) |
| 15 | 28 V Ground | -- | Ground |

### Discrete Signals

The three GPIO inputs sampled on the BTC are:

| Signal | Direction | Polarity | Role |
|--------|-----------|----------|------|
| LO (Lift-Off) | RXSM -> BTC | Rising edge defines `t = 0` | First rising edge after boot transitions the BTC out of pre-flight wait state. The tick at which LO is observed is the science timeline reference for all controllers. |
| SODS (Start of Data Storage) | RXSM -> BTC | Active level reported | Sampled and reported in `PayloadBtcStatus::signal_mask` bit 2 each tick. Candidate trigger for the start of SD logging; final mechanism still to be defined. SODS is documented as susceptible to noise, so any use as a control input will need debouncing. |
| SOE (Start of Experiment) | RXSM -> BTC | Active level reported | Sampled and reported in `signal_mask` bit 1 each tick. Recorded for post-flight correlation only. |

All three signals are sampled once per tick from inside the tick
loop. We use level sampling (not interrupts) for SODS and SOE to
keep the tick body deterministic. LO uses an interrupt to capture
the precise rising-edge microsecond timestamp, which is recorded
and downlinked in the BTC status payload.

### Cable Specification

Per REXUS User Manual section 5.2 for the D-SUB feedthrough.
Shielded twisted pair for both RS-422 directions. Maximum cable
length within the REXUS module is well under 5 m, so the RS-422
range is not a limiting factor.

## Protocol Specification

### General Format

The BTC writes downlink frames as a continuous byte stream over the
UART. Each frame is a complete THHOR-BOLT downlink packet (see
[ICD-006](ICD-006-downlink-packet-format.md)) with a two-byte sync
word `0xB0 0x17` at the start and a CRC-16/CCITT at the end. Total
frame length is 14-64 bytes.

The receiver re-syncs by scanning for the sync word and verifying
the CRC. Bytes between frames are silence (idle line).

### Timing Requirements

| Parameter | Value | Notes |
|-----------|-------|-------|
| Nominal aggregate rate | ~83 kbit/s | All sources at planned cadence |
| Maximum aggregate rate | <= 115 kbit/s | Burst budget for recovery and BOOT packets |
| Minimum acceptable rate | 50 kbit/s | Minimum-success criterion |
| Frame inter-arrival | >= 0 | No required gap; back-to-back frames are valid |
| LO detection latency | <= 40 ms | Sampled once per tick |

The BTC drains its outgoing CAN-reassembled queue at the start of
each tick body and emits frames on the RS-422 UART. The UART
transmits in DMA-circular mode so the tick body does not block on
transmit; if the queue is empty the line stays idle.

### Identifier Assignment

Not applicable. RS-422 is a point-to-point byte stream; no
addressing. Frame routing on ground is by `PayloadType` inside the
packet header.

## Message Definitions

There are no separate "messages" at the RS-422 layer; every byte is
part of a THHOR-BOLT packet. See [ICD-006](ICD-006-downlink-packet-format.md)
for the packet format and [ICD-007](ICD-007-packet-payloads.md) for
the per-type payload definitions.

The packet types that flow over this link are the union of all
sources:

| Source | Types |
|--------|-------|
| BTC | `BTC_ENV` (0x10), `BTC_STATUS` (0x11) |
| EXP1 | `EXP1_ENV` (0x20), `EXP1_STATUS` (0x21), `EXP1_SPECTRUM_A` (0x22), `EXP1_SPECTRUM_B` (0x23) |
| EXP2 | `EXP2_ENV` (0x30), `EXP2_STATUS` (0x31), `EXP2_BER` (0x32) |
| EXP3 | `EXP3_ENV` (0x40), `EXP3_STATUS` (0x41), `EXP3_STACK_A` (0x42), `EXP3_STACK_B` (0x43) |
| System | `GAP_MARKER` (0xF0), `BOOT` (0xFE) |

## Error Handling

### Corruption Detection

Each packet carries a CRC-16/CCITT-FALSE (polynomial `0x1021`,
seed `0xFFFF`) over header + payload, excluding the sync word.
The ground station discards packets whose CRC does not match and
counts them for monitoring.

### Recovery from Corruption

No application-layer retransmission. The downlink is unidirectional
from the BTC's perspective. The science survives on SD per I-7
([ADR-009](../decisions/ADR-009-raw-sensor-data-to-ground.md)).

### Missing Signals

If LO is not observed within the expected pre-flight window, the
BTC continues to run its tick loop and accumulates ground-phase
data (T-) on SD without producing the LO-timestamp portion of the
status payload. The science timeline only starts on LO.

If the RS-422 UART hardware reports a TX error, the BTC counts the
error in its status payload and continues. There is no recovery path
mid-flight; the SD log remains authoritative.

## Constants and Limits

| Constant | Value | Description |
|----------|-------|-------------|
| `RS422_BAUDRATE` | 115 200 | Default UART baud rate |
| `MAX_PACKET_SIZE` | 64 | Maximum bytes per packet |
| `SYNC_0`, `SYNC_1` | 0xB0, 0x17 | Frame sync word |

## Test Vectors


### Invalid Packet (Corrupted CRC)

A packet whose payload bit was flipped in transit produces a CRC
mismatch on ground. Expected behaviour: the ground decoder
increments its corruption counter and resumes scanning for the next
sync word.

## Implementation Notes

The RS-422 transceiver IC and the EMI filtering on the BTC side are
documented in the CDR electronics chapter; this ICD covers only the
logical interface.

The BTC reassembles fragmented CAN payloads (see
[ICD-002](ICD-002-can-protocol.md)) before forwarding to RS-422.
Each reassembled experiment payload becomes one outgoing frame.

## References

- Related ADRs:
  - [ADR-001 Tick Architecture](../decisions/ADR-001-tick-architecture.md)
  - [ADR-010 Packet Format](../decisions/ADR-010-packet-format-crc16.md)
- Related ICDs:
  - [ICD-002 CAN Protocol](ICD-002-can-protocol.md) (upstream of this link)
  - [ICD-006 Downlink Packet Format](ICD-006-downlink-packet-format.md)
  - [ICD-007 Packet Payloads](ICD-007-packet-payloads.md)
- Standards:
  - TIA/EIA-422-B
  - REXUS User Manual section 5.2 (D-SUB feedthrough)
  - REXUS PCM Telemetry System RX00-101006
- Implementation:
  - [`flight-software/btc/app/`](../../flight-software/btc/app/)
  - [`flight-software/shared/comms/packet_builder.hpp`](../../flight-software/shared/comms/packet_builder.hpp)