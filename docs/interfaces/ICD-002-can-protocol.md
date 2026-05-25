# ICD-002: CAN Protocol BTC-to-Experiments

## Document Information

| Field | Value |
|-------|-------|
| Document ID | ICD-002 |
| Version | 1.0 |
| Status | Approved |
| Date | 2026-05-15 |
| Owner | Software Lead |
| Reviewers | Hardware Lead |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-05-15 | Max | Initial draft from CDR |
| 1.0 | 2026-05-25 | Max | Approved for CDR |

## Purpose

Defines the CAN bus protocol used between the Bifröst Test Controller 
(BTC) and the three experiment controllers (EXP1, EXP2, EXP3) in the 
THHOR-BOLT REXUS 37 mission. The protocol distributes timing 
synchronisation from the BTC to all experiments at 25 Hz and aggregates 
experiment telemetry into the downlink stream.

## Scope

This document specifies:

- Physical CAN bus configuration
- Message identifier assignment
- SYNC packet format and timing
- Experiment publication messages
- Fragmentation protocol for messages exceeding 8 bytes
- Error handling for CAN frame corruption

This document does not cover:

- The downlink packet format used after BTC reassembly (see ICD-001)

## Parties

| Component | Hardware | Role |
|-----------|----------|------|
| BTC | STM32F756ZG | Bus master, SYNC broadcaster, downlink aggregator |
| EXP1 | STM32L476RGT6 | Space Disco data publisher |
| EXP2 | STM32L476RGT6 | Bouncy Castle data publisher |
| EXP3 | STM32L476RGT6 | Floaty Boi data publisher |

## Physical Layer

### Electrical Specification

To be defined

### Cable Specification

To be defined

## Protocol Specification

### General Format

All messages use CAN 2.0B standard frames with 11-bit identifiers. 
Payload size 0 to 8 bytes per frame. Messages exceeding 8 bytes are 
fragmented at the application layer (see Fragmentation section).

Byte order is little-endian for multi-byte fields.

### Timing Requirements

| Parameter | Value | Notes |
|-----------|-------|-------|
| SYNC broadcast rate | 25 Hz | Every 40 ms |
| SYNC jitter tolerance | +-5 ms | Experiments must accept |
| Max frames per experiment per tick | 12 | Prevents bus saturation |
| Max bus utilisation | 50% | Headroom for retries |

### Identifier Assignment

| ID Range | Owner | Purpose |
|----------|-------|---------|
| 0x000 - 0x00F | BTC | Broadcast messages (SYNC, time, mode) |
| 0x010 - 0x0FF | Reserved | Future BTC use |
| 0x100 - 0x1FF | EXP1 | EXP1 publications |
| 0x200 - 0x2FF | EXP2 | EXP2 publications |
| 0x300 - 0x3FF | EXP3 | EXP3 publications |

## Message Definitions

### Message: SYNC

| Field | Value |
|-------|-------|
| ID | 0x001 |
| Direction | BTC -> All |
| Length | 8 bytes |
| Frequency | 25 Hz |
| Purpose | Distribute BTC tick reference and mission phase |

**Payload Structure:**

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 4 B | uint32 LE | tick_number | Current BTC tick from LO |


## Fragmentation Protocol

Messages longer than 8 bytes are split across multiple CAN frames. The 
first byte of each fragment is a fragment header:

| Bits | Field | Description |
|------|-------|-------------|
| 7 | More fragments | 1 if more follow, 0 if last |
| 6-4 | Sequence | Fragment sequence (0-7) |
| 3-0 | Reserved | Set to 0 |

The remaining 7 bytes of each fragment carry payload data. The receiver 
reassembles fragments by sequence number until receiving a frame with 
the "more fragments" bit cleared.

## Error Handling

### Corruption Detection

CAN frame CRC is verified by hardware. Frames that fail CRC are reported 
to the application via the peripheral's error status. The application 
counts these errors in the `can_crc_errors` field of the BTC status 
payload.

### Recovery from Corruption

Corrupted frames are discarded silently. No retransmission is requested 
at the application layer. Lost data is reported to ground via a 
GAP_MARKER packet with reason `CAN_CRC_FAIL`.

### Missing Messages

If an experiment misses 5 consecutive SYNC messages (200 ms), it enters 
autonomous mode (see ADR-004 Fault Management).

## Constants

| Constant | Value | Description |
|----------|-------|-------------|
| SYNC_PERIOD_MS | 40 | Time between SYNC broadcasts |
| MAX_FRAGMENTS_PER_MESSAGE | 8 | Maximum fragments per logical message |
| AUTONOMOUS_MODE_THRESHOLD | 5 | Missed SYNCs triggering autonomous mode |

## Test Vectors

to be defined