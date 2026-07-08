# ADR-012: Error step trace in `std::expected` + FAULT packet

**Status:** Proposed
**Date:** 2026-07-08

## Context

All fallible flight code returns `Result<T> = std::expected<T, Error>`,
where `Error` was a bare uint8 enum. A latched fault reached the ground
as a `SENSOR_FAILED` gap marker with the LED fault code smuggled into
`first_missing_tick` — it said *that* a device failed, but not *where
in the call chain*, which of a driver's many bus transactions, or
*when*. Debugging a flight anomaly from that is guesswork.

## Decision

1. **`Error` becomes a record that carries its own trace.** Fields:
   cause (`ErrorCode`), fixed `trace[6]` of `Step` entries
   (`trace[0]` = origin, then outward), `depth`, `truncated`, origin
   `__LINE__` (uint16) and origin `timestamp_us`.
   - `fail(code, step, __LINE__)` creates the origin (stamps the time
     via the `ErrorClock` hook, set once at boot to the platform µs
     clock).
   - `mark(err, step)` is called by each *semantic* level while the
     expected propagates outward; when the array is full it sets
     `truncated` instead of writing past the end.
   - Pure pass-through helpers do not mark, keeping real chains within
     depth 6 (deepest observed: origin → bus API → driver helper →
     driver op → caller).
2. **Fixed depth 6, no allocation.** A growing container would cost
   heap (banned in the tick loop) and unbounded WCET. 9 bytes of trace
   in every `Result` error path is the deal; `sizeof(Error) <= 16` is
   static-asserted.
3. **`__LINE__` as an explicit parameter** — no macro (clang-tidy), and
   no `std::source_location`, which would embed file/function strings
   into flash at every origin site.
4. **Latching devices keep their death trace.**
   `DeviceBase::register_failure(err)/disable(err)` store the causing
   `Error`; polled reporting (`poll_device_fault()`) ships it later
   with exact origin time and line.
5. **New FAULT packet (0xF1)** replaces the SENSOR_FAILED gap-marker
   convention: fault code (= LED blink code), error code,
   depth/truncated, origin line, 6 steps. Header timestamp = moment of
   occurrence; header tick = report time. `GapReason::SENSOR_FAILED`
   stays wire-stable but is no longer emitted. Dictionary:
   docs/handbook/fault-trace-codes.md (append-only values).
6. The report hook was renamed `on_sensor_failed` →
   **`report_fault`**: UART/CAN ring latches use the same path and are
   not sensors.