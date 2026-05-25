# ICD-006: Downlink packet format

## Document Information

| Field | Value |
|-------|-------|
| Document ID | ICD-006 |
| Version | 1.0 |
| Status | Approved |
| Date | 2026-05-25 |
| Owner | Communications Lead |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-05-15 | Max | Initial draft from CDR section 4.5 |
| 1.0 | 2026-05-25 | Max | Approved for CDR |

## Purpose

Wire ABI for every downlink packet. Same format produced by all four
controllers, transported over CAN (after fragmentation) and RS-422,
and decoded on the ground side.

Per-payload-type fields are in [ICD-007](ICD-007-packet-payloads.md);
this file is the shared header, framing, and CRC.

## Layout

Total: 14-64 bytes.

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 2 | `sync` | `0xB0 0x17`, excluded from CRC |
| 2 | 1 | `version` | `0x01` |
| 3 | 1 | `type` | `PayloadType` |
| 4 | 1 | `sequence` | per-type counter, wraps at 255 |
| 5 | 1 | `length` | payload length, 0..50 |
| 6 | 2 | `tick` | uint16 LE, 25 Hz tick since LO |
| 8 | 4 | `timestamp_us` | uint32 LE, us since LO |
| 12 | N | `payload` | up to 50 B |
| 12+N | 2 | `crc16` | big-endian on the wire |

```
+----+---+---+---+---+------+--------+---------+------+
|sync|ver|typ|seq|len|tick  |ts_us   |payload  |CRC16 |
| 2  | 1 | 1 | 1 | 1 | 2 LE | 4 LE   |  0..50  | 2 BE |
+----+---+---+---+---+------+--------+---------+------+
       \------ CRC scope (sync excluded) -----/
```

Constants in [`packet_header.hpp`](../../flight-software/shared/comms/packet_header.hpp):
`MAX_PACKET_SIZE = 64`, `HEADER_SIZE = 12`, `CRC_SIZE = 2`,
`MAX_PAYLOAD = 50`.

### Byte order

- Header numeric fields and payload fields: **little-endian**.
- CRC: **big-endian** on the wire.

This asymmetry is a historical RS-422 convention. Yes it's a
footgun. Canonical encoder: `PacketBuilder::build()`.

## CRC

CRC-16/CCITT-FALSE.

- Polynomial `0x1021`
- Initial value `0xFFFF`
- No input/output reflection
- No final XOR
- Scope: bytes 2 .. (12 + length - 1) -- header without sync, plus
  payload

Reference: [`crc16.hpp`](../../flight-software/shared/utils/crc16.hpp).
`constexpr` so unit tests run it at compile time.

## Sync Word and Re-Sync

A valid packet must:

1. Start with `0xB0 0x17`.
2. Have `version == 0x01`.
3. Have `length <= 50`.
4. Pass the CRC.

If any check fails, the receiver shifts forward one byte and scans
for the next sync candidate. False matches on the sync word are
caught by the CRC at the end of the candidate.

## Sequence and Time

- `sequence` is per-`PayloadType`, wraps at 255. Lets ground detect
  drops inside a stream.
- `tick`: 25 Hz counter. T- before LO, T+ after.
- `timestamp_us`: tick-start us from each controller's DWT cycle
  counter.

System packets sent before LO (`BOOT`, early `GAP_MARKER`) use
`tick = 0`, `timestamp_us = 0`.

## Size Budget

64-byte total cap is a deliberate budget anchor. It bounds CAN
fragmentation worst case (~15 frames x 7 B payload + framing
overhead) and the RS-422 bandwidth budget.

The header is 12 B which is a big chunk of small packets. Long
payloads like EXP1 spectrum are split A/B. The ground decoder
re-pairs by tick.

## Compile-Time Checks

Every payload struct passes `PACKET_TYPE_CHECK(T)` in
[`packet_payloads.hpp`](../../flight-software/shared/comms/packet_payloads.hpp):

- `std::is_trivially_copyable<T>`
- `std::is_standard_layout<T>`
- `sizeof(T) <= MAX_PAYLOAD`

A struct that fails fails the build.

## Test Vectors

### Re-sync

Receive buffer: `DE AD BE EF B0 17 01 FE ...`

Decoder discards `DE AD BE EF`, finds the sync at offset 4, reads
`01` as version and proceeds.

### Empty payload (BOOT, cold start)

```
B0 17 01 FE 00 04 00 00 00 00 00 00 01 00 00 00 CRC CRC
```

`PayloadBoot { reason = COLD_START, reboot_count = 0 }`, tick = 0,
ts_us = 0. CRC bytes are illustrative; canonical encoder is
`PacketBuilder::build_boot()`.

## Notes

`PacketBuilder::build()` writes into a caller-owned buffer. No
allocation. Returns total length or 0 on overflow.

Per-type sequence counters live in a 256-entry array. Most slots are
unused. Cheaper than a hash map and predictable in size (I-3).

`PayloadGapMarker` and `PayloadBoot` are normal payload types; no
parallel framing path.


## References

- [ADR-010 Packet Format](../decisions/ADR-010-packet-format-crc16.md)
- [ADR-009 Raw Sensor Data to Ground](../decisions/ADR-009-raw-sensor-data-to-ground.md)
- [ICD-001 RS-422](ICD-001-rs-422-to-rxsm.md)
- [ICD-002 CAN Protocol](ICD-002-can-protocol.md)
- [ICD-007 Packet Payloads](ICD-007-packet-payloads.md)
- CRC: CRC-16/CCITT-FALSE (ITU-T V.41)
- [`packet_builder.hpp`](../../flight-software/shared/comms/packet_builder.hpp)
