# Adding a Driver

Device class, recovery, telemetry, fault code, self-test: the full
sequence for integrating new hardware on any of the four controllers.
Written for people and assistants alike; `.claude/skills/add-driver`
points here.

The shape is fixed; a driver that skips a part of it leaves a hole in
recovery, telemetry or diagnosis.

## 1. Device class

`shared/sensors/<chip>.{hpp,cpp}` (or `shared/led/`), inheriting
`DeviceBase`: it provides the failure latch, `retry_due()/arm_retry()/
clear_latch()` and the 30 s retry cooldown. All bus access through
`CmsisI2CBus` (bounded timeouts, I-2; it already hardens against clock
stretching and stuck transfers). Return `Result<T>`/`std::expected`
with a new `Step` code range - append after the existing ranges in
`shared/utils/errors.hpp`, never renumber, and update
`docs/handbook/fault-trace-codes.md` in the same commit.

## 2. Wire it into the node

- Common to all nodes: member + init in `NodeComputer::init_sensors`,
  retry block in `retry_failed_devices` (own helper function, see the
  baro/tmp ones). Node-specific: the concrete computer +
  `init_extra_sensors`/`retry_extra_devices` (the BTC `ImuSupervisor`
  is the template when the device needs its own policy).
- Init failure: `disable(err)` the device, set the LED fault, continue
  booting. A missing sensor costs its data, never the node (P-1).

## 3. Telemetry (I-6)

Every reading is gated: a `valid_mask` bit in an existing payload or a
new payload via
[changing-the-wire-format.md](changing-the-wire-format.md). Never
downlink a stale or default value as valid. Raw counts go to ground;
conversion happens in the decoder (ADR-009), so the `WIRE(.scale)`
annotation needs a datasheet-checked value, named in the PR.

## 4. Fault code

Append the next `StatusLeds::Fault` value (the number IS the LED pulse
count, wire-stable, same value as `fault_code` in the FAULT packet).
Update the table in `status_leds.hpp`, `docs/handbook/led-blink-codes.md`
and the SED LED-codes section.

## 5. Self-test step

Append steps AFTER the node's existing table (test_id = table index,
wire-stable; the three common sensor steps come first on every node).
Pattern: an identity step (WHO_AM_I / PROM CRC) and a plausibility read;
return `SKIPPED` when the device is latched failed; `data` carries the
raw diagnostic u32, the ground judges it.

## Bench realities (cost us days; check before debugging software)

- ICM-4268x: AD0 floats on rev-A BTC (cold-boot NACK; strap to 3V3 for
  flight), soft reset + WHO_AM_I retry needed on cold boot.
- AS7265x: CONTROL bit 7 is RST, not INT; never reuse the virtual-reg
  pointer for polling; probe the virtual-register handshake, not a bare
  ACK, when waiting for boot.
- LP5810: real EXP1 addresses 0x58 (C) / 0x5C (D); ICD-005 still says
  0x14/0x15 and is stale.
- All rev-A boards run a 16 MHz HSE crystal; a 48 MHz clock config
  "works" 3x slow. Check the clock tree before trusting any timing.

## Gate

`./scripts/verify.sh` - and a co-located Catch2 host test
(`<unit>.test.cpp` next to the driver) for any logic that runs on
the host (CRC, parsing, state machines).
