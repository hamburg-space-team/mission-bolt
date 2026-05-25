# ADR-002: Class hierarchy for the four flight binaries

## Status

Accepted

## Date

2026-05-13

## Context

Four flight binaries (BTC + EXP1/2/3) do mostly the same thing every
tick -- sample MS5611 + TMP117, build packet, log, kick watchdog --
and diverge only in transport role and experiment logic.

Four copy-pasted codebases mean four versions of the safety-critical
tick loop. One runtime-parameterised binary hides which paths run
where. Neither is OK.

We want the loop, packet code, sensor drivers, and SD layer written
*once* and verified centrally, while still producing four distinct
images.

## Decision

```
FlightComputer (abstract)         tick loop, on_init/on_tick
    +-- NodeComputer (abstract)   MS5611, TMP117, SD, LEDs, BootState
            +-- BtcComputer       CAN master, RS-422, LO/SODS/SOE
            +-- ExpComputer (abstract)   CAN slave framework
                    +-- Exp1Computer    Space Disco
                    +-- Exp2Computer    Bouncy Castle
                    +-- Exp3Computer    Floaty Boi
```

Leaf classes add 30-80 lines on top of the shared base. All
classes `final` where they shouldn't be extended, all non-copyable
and non-movable. Instances are static; one per controller.

## What we looked at

- **Four parallel codebases** -- bug fixes would need four PRs.
- **Single runtime-configurable binary** -- hides paths that can
  actually run on a given controller, fights I-3.
- **Templates instead of inheritance** -- pushes the difference into
  types, makes stack traces noisier, no real win.

## Consequences

Good:
- Shared paths exist once. Bug fixes propagate automatically.
- You can tell from the class name which controller it is.
- `Platform` abstraction lets us unit-test the loop host-side.

Bad:
- Virtual dispatch on `on_init()`/`on_tick()` once per tick. At
  25 Hz this is unmeasurable but real.

## Notes

`FlightComputer::run()` is `[[noreturn]]`. Each `app_main()` builds
a `Platform`, the computer, and calls `run()`.

`tx_buf` in `NodeComputer` is the single 64 B staging buffer used
for both CAN frames and RS-422 packets.

## References

- [ADR-001 Tick Loop](ADR-001-tick-architecture.md)
- [ADR-008 .noinit Recovery](ADR-008-noinit-ram-recovery.md)
- Code under [`shared/core/`](../../flight-software/shared/core/)
