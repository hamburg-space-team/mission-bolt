# ADR-003: LittleFS on microSD

## Status

Accepted

## Date

2026-05-13

## Context

Every controller logs to a local microSD card. The SD log is the
authoritative science record (I-7). Two things will happen:
the card stalls during internal GC, and we get a reset mid-write
(e.g. watchdog).

We need power-fail-safe semantics so that "half a write" never
becomes corrupt science data.

## Decision

**LittleFS v2.11.3** over the STM32 SDMMC peripheral. Block-device
callbacks adapt LittleFS to SDMMC sectors.

Config:

- `block_size = read_size = prog_size = cache_size = 512`
- `lookahead_size = 16`
- `block_cycles = 500`

One append-only log file (`log.bin`) per controller, opened at
boot. `SdStore::write()` appends, `SdStore::flush()` calls
`lfs_file_sync()` on the cadence defined in
[ADR-004](ADR-004-storage-ringbuffer.md).

If the mount fails we format once and retry. Three consecutive
write errors latch a permanent failure flag; later writes are
suppressed and reported in `PayloadBtcStatus`.

## What we looked at

- **FAT (FatFs)** -- not power-fail-safe; mid-write reset can leave
  the FAT and directory entry inconsistent. Native readability not
  worth the risk.
- **Custom append-only log** -- we'd have to prove the power-fail
  properties ourselves. LittleFS already has them.

## Consequences

Good:
- Power-fail-safe by construction; rolls back to the last sync.
- 512-byte cache aligns with SD sector size.
- One simple API (`write`, `flush`) hides LittleFS from callers.

Bad:
- Desktop OSes don't read LittleFS without tooling. Ground recovery
  needs the LittleFS Python tool.
- GC stalls are still real, just absorbed off the critical path
  ([ADR-004](ADR-004-storage-ringbuffer.md)).

## Notes

We trigger GC deliberately twice: once at boot and once right after
LO is observed by the BTC. This pre-empts the worst stalls
landing mid-science.

`block_cycles = 500` is the LittleFS default. Wear is irrelevant
at our log volumes (single-digit MB per flight) but leaving the
default in keeps us boring.

LittleFS code lives under `flight-software/lib/littlefs/`, pinned;
we don't touch it.

## References

- [ADR-004 Storage Ringbuffer](ADR-004-storage-ringbuffer.md)
- [ADR-005 Fault Management](ADR-005-fault-management.md)
- LittleFS: <https://github.com/littlefs-project/littlefs>
- [`shared/storage/sd_store.hpp`](../../flight-software/shared/storage/sd_store.hpp)
