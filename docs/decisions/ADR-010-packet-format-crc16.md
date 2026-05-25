# ADR-010: 64-byte packet format with CRC-16/CCITT

## Status

Accepted

## Date

2026-05-13

## Context

Telemetry has to travel from each controller through CAN, into the
BTC, out the RS-422 link, and onto ground. One packet format on
both sides keeps the wire ABI from going off the rails. We need
re-sync after byte loss, corruption detection beyond REXUS-level
FEC, and a layout we can build in constant time with no allocation
(I-3).

## Decision

Every packet has a fixed 12 B header, a variable payload, and a
2 B trailing CRC. Total <= **64 B**.

```
sync(2) ver(1) type(1) seq(1) len(1) tick(2 LE) ts_us(4 LE)
                                            payload(0..50) CRC16(2 BE)
```

`sync = 0xB0 0x17` (excluded from CRC). `ver = 0x01`. CRC is
**CRC-16/CCITT-FALSE** (poly `0x1021`, init `0xFFFF`, no reflection),
covering bytes 2..end-of-payload, written big-endian.

Header constants are in [`packet_header.hpp`](../../flight-software/shared/comms/packet_header.hpp):
`MAX_PACKET_SIZE = 64`, `HEADER_SIZE = 12`, `CRC_SIZE = 2`,
`MAX_PAYLOAD = 50`.

`PayloadType` is one byte, grouped by source. Per-type sequence
counter wraps at 255. Tick + microsecond timestamp let ground
correlate across the four controllers.

Full payload list and per-type layouts: [ICD-007](../interfaces/ICD-007-packet-payloads.md).

## What we looked at

- **COBS framing** -- extra escaping state machine for not much
  benefit.
- **CCSDS Space Packet** -- header alone is bigger; we don't gain
  anything from APID semantics here.
- **CRC-32** -- doubles overhead per packet, REXUS FEC already
  covers the radio layer; CRC-16 is enough.
- **No CRC** -- CAN-to-RS-422 reassembly is software, we want a
  cross-check.

## Consequences

Good:
- Compile-time guarantees via `PACKET_TYPE_CHECK`: every payload
  struct is trivially copyable, standard layout, <= 50 B.
- Receivers re-sync by scanning for `0xB017` and validating CRC.
- One `PacketBuilder` per controller; no allocator, no second writer.

Bad:
- 12 B header is a big fraction of small packets.
- 64 B cap forces A/B splitting for EXP1 spectrum.
- Mixed-endian (header LE, CRC BE) is a footgun. Documented here
  and in [ICD-006](../interfaces/ICD-006-downlink-packet-format.md).

## Notes

`PacketBuilder::build()` returns total length or 0 on overflow. No
allocation. The same builder runs for CAN (after reassembly inside
the BTC) and RS-422.

The 256-entry sequence array is wasteful but predictable in size.
We trade ~256 B of RAM for "no hash map" (I-3).

CRC `compute()` is `constexpr` -- unit tests run it at compile time.

## References

- [ADR-005 Fault Management](ADR-005-fault-management.md)
- [ADR-009 Raw Sensor Data to Ground](ADR-009-raw-sensor-data-to-ground.md)
- [ICD-006 Packet Format](../interfaces/ICD-006-downlink-packet-format.md)
- [ICD-007 Packet Payloads](../interfaces/ICD-007-packet-payloads.md)
- [`packet_builder.hpp`](../../flight-software/shared/comms/packet_builder.hpp)
