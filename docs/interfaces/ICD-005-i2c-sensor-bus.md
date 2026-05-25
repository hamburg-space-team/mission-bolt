# ICD-005: I2C sensor bus on the flight controllers

## Document Information

| Field | Value |
|-------|-------|
| Document ID | ICD-005 |
| Version | 1.0 |
| Status | Approved |
| Date | 2026-05-25 |
| Owner | Software Lead |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-05-15 | Max | Initial draft from CDR section 4.5 |
| 1.0 | 2026-05-25 | Max | Approved for CDR |

## Purpose

I2C bus configuration and address map for each STM32L476 flight
controller. Same peripheral, same timing, same timeout policy across
all four boards. Only the device list differs.

The sensors on the EXP3 stacks live on the local stack MCU's bus,
not the EXP3 controller's; they reach the controller over UART per
[ICD-004](ICD-004-uart-to-lifi-exp3.md).

## Bus Config

| Parameter | Value |
|---|---|
| Peripheral | I2C1 (master) |
| Bit rate | 400 kHz (Fast Mode) |
| Kernel clock | PCLK1 = 16 MHz |
| Timing register | `0x00100D14` (CubeMX) |
| Addressing | 7-bit |
| Driver | CMSIS-Driver I2C |
| Voltage | 3.3 V |
| Pull-ups | 4.7 kOhm to 3.3 V on each board |

Wrapped by [`CmsisI2CBus`](../../flight-software/shared/bus/cmsis_i2c_bus.hpp).

## Address Map

| Controller | Sensor | 7-bit Addr |
|---|---|---|
| BTC  | TMP117 | `0x48` |
| BTC  | MS5611 | `0x77` |
| BTC  | ICM-42686-P | `0x68` |
| EXP1 | TMP117 | `0x48` |
| EXP1 | MS5611 | `0x77` |
| EXP1 | AS7265X | `0x49` |
| EXP1 | LP5810 (C, RGB) | `0x14` |
| EXP1 | LP5810 (D, W/IR/UV) | `0x15` |
| EXP2 | TMP117 | `0x48` |
| EXP2 | MS5611 | `0x77` |
| EXP3 | TMP117 | `0x48` |
| EXP3 | MS5611 | `0x77` |
| EXP3 | ICM-42686-P | `0x68` |

No conflicts: each board has its own bus.

## Polling Cadence

| Device | Cadence | Notes |
|---|---|---|
| TMP117 | every tick (25 Hz) | One-shot conversion register |
| MS5611 | every tick | D1+D2 pipelined across ticks to amortise the 10 ms conversion |
| ICM-42686-P | every tick | accel + gyro pair |
| AS7265X | multi-tick cycle | driven by the EXP1 measurement state machine |
| LP5810 | on state change | not polled |

## Timing & Timeouts

- Per-transaction timeout: **10 ms**. Hard. Driver returns an error
  if exceeded.
- Per-tick aggregate I2C time: target <= 5 ms. TODO: measure on the
  flight board with all sensors live.
- A failed transaction counts toward the three-strike rule in
  [ADR-005](../decisions/ADR-005-fault-management.md). No retries
  in the same tick.

## Sensor scaling

Drivers return raw register values. Conversion happens on ground
([ADR-009](../decisions/ADR-009-raw-sensor-data-to-ground.md)). The
per-payload `valid_mask` reflects per-sensor success.

| Sensor | Driver location | Comment |
|---|---|---|
| MS5611 | `shared/sensors/ms5611.cpp` | Driver checks PROM CRC on init |
| TMP117 | `shared/sensors/tmp117.cpp` | 1 LSB = 1/128 degC |
| ICM-42686 | `shared/sensors/icm42686.cpp` | +-32 g, +-2000 dps |
| AS7265X | `shared/sensors/as7265x.cpp` | Has its own reset-recovery path |
| LP5810 | `shared/led/lp5810.cpp` | Write-only |

## Error Handling

The bus driver surfaces NACK and timeout. The sensor driver calls
`register_failure()` / `clear_failures()` on its `DeviceBase`.

After 3 consecutive failures the device is disabled for the rest of
flight. The corresponding `valid_mask` bit stays cleared so ground
sees the transition.

If the I2C peripheral itself reports a bus error (arbitration loss,
SCL stuck low), the driver software-resets the peripheral. The bus
stays usable for the rest of the tick; the device that triggered it
takes one strike.

AS7265X gets an extra reset-pin recovery before the three-strike
rule kicks in -- see [ADR-005](../decisions/ADR-005-fault-management.md).

## Notes

`DeviceBase` is the shared book-keeper for failure state. Every
sensor driver inherits it and must call its hooks consistently;
forgetting `clear_failures()` after a success will eventually
disable a healthy sensor.

We do not retry transactions in the driver. Retries hide the true
failure shape and the three-strike rule already gives transient
errors room.

## References

- [ADR-005 Fault Management](../decisions/ADR-005-fault-management.md)
- [ADR-009 Raw Sensor Data to Ground](../decisions/ADR-009-raw-sensor-data-to-ground.md)
- [ICD-007 Packet Payloads](ICD-007-packet-payloads.md)
- [`shared/bus/cmsis_i2c_bus.hpp`](../../flight-software/shared/bus/cmsis_i2c_bus.hpp)
- [`shared/utils/device_base.hpp`](../../flight-software/shared/utils/device_base.hpp)
