# Reviewing Firmware Changes

Checklist for any change under `flight-software/` that touches tick
bodies, drivers, comms, or state handling. Written for people and
assistants alike; `.claude/skills/firmware-review` points here.

Cite invariants by number (docs/standards/system-invariants.md). The
checks below are the ones that catch real defects in this codebase.

## Per invariant

- **I-1 tick budget (40 ms, target 35).** New work in a tick body needs
  a place in the budget; wrap measurable phases in `BOLT_TIME(timings,
  SCOPE)` so the per-node `*_TIMING` telemetry sees it in flight. No
  busy-waits, no unbounded loops.
- **I-2 bounded blocking.** Every I2C/UART/CAN call takes a timeout.
  `HAL_*` and driver return values are checked, never discarded. SD
  writes are the documented exception (ring buffer, drained off the
  critical path).
- **I-3 no dynamic allocation.** No `new`, `vector`, `string`,
  `function`, no hidden allocations (e.g. `std::function` captures).
  The `invariants` verify stage rejects allocating `operator new` in the
  image; it cannot see newlib `malloc` reachability, so review still
  looks for printf-family additions on the flight path.
- **I-4 monotonic timeline.** `sync_count` only moves forward except at
  the LO epoch reset. Anything persisted for warm recovery lives in the
  `.noinit` block (`BootState`); a state that must survive a mid-flight
  reset but is not in that block is a bug (the LO latch was exactly
  this).
- **I-5 loose coupling.** An EXP must keep working when the BTC dies:
  autonomous fallback keeps the last mode, never invents FLIGHT. SYNC
  carries levels, not events; missing one frame costs a tick, never
  state.
- **I-6 no silent substitution.** A failed read sets its `valid_mask`
  bit or produces a `GAP_MARKER` with a reason. Never a stale or default
  value in a valid-marked field.

## House patterns to enforce

- Errors: `std::expected` + step trace. `Step` codes, `StatusLeds::Fault`
  codes and self-test `test_id`s are wire-stable: append in the owning
  range, never renumber, update `docs/handbook/fault-trace-codes.md` in
  the same commit.
- Devices: one-shot fault report + latch + 30 s cooldown retry
  (`DeviceBase`). A mode change never clears device health.
- Downcasts in self-test step adapters use
  `// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)`
  on its own line. A trailing NOLINT gets detached by clang-format, after
  which the tidy autofix flips `static_cast` to `dynamic_cast` and breaks
  the `-fno-rtti` build.
- ISR handoff: `volatile` flag + payload; writer stores payload then
  sets the flag, reader tests flag, reads payload, clears flag. Lossy
  one-shots stay one-shots.
- Time: `timestamp_us` wraps every ~53.7 s (2^32 cycles at 80 MHz) and
  is a stamp, not a timeline. Durations subtract in the cycle domain
  (`ErrorClock::us_between`); ordering and plotting use the tick.
- Layer boundary: `btc/app/btc` and `btc/app/protocol` never include
  `main.h`/`stm32*`; HAL lives behind the `board/` seam. The `layers`
  check enforces it.

## Gate

`./scripts/verify.sh` before claiming done; at minimum `flight-build
host-tests invariants lint-flight` for firmware-only changes.
