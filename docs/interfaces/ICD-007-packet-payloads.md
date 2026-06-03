# ICD-007: Downlink packet payloads

## Document Information

| Field | Value |
|-------|-------|
| Document ID | ICD-007 |
| Version | 1.0 |
| Status | Approved |
| Date | 2026-05-25 |
| Owner | Communications Lead |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-05-15 | Max | Initial draft from `packet_payloads.hpp` |
| 1.0 | 2026-05-25 | Max | Approved for CDR |

## Purpose

Per-type binary payload definitions. Combined with
[ICD-006](ICD-006-downlink-packet-format.md), this file is the
complete wire ABI between flight and ground.

The reference implementation is
[`packet_payloads.hpp`](../../flight-software/shared/comms/packet_payloads.hpp).
When in doubt, the header file wins; this document tracks it.

## Conventions

- All multi-byte numeric fields in payloads are **little-endian**
  unless flagged otherwise.
- `valid_mask` is a per-payload byte with one bit per sensor in
  that payload; cleared bit = read failed or sensor disabled.
- `reserved` bytes are zero on producers, ignored on consumers.

## PayloadType Enumeration

| Value | Name | Struct | Producer |
|---|---|---|---|
| `0x10` | `BTC_ENV` | `PayloadBtcEnv` | BTC |
| `0x11` | `BTC_STATUS` | `PayloadBtcStatus` | BTC |
| `0x20` | `EXP1_ENV` | `PayloadExpEnv` | EXP1 |
| `0x21` | `EXP1_STATUS` | `PayloadExpStatus` | EXP1 |
| `0x22` | `EXP1_SPECTRUM_A` | `PayloadExp1SpectrumA` | EXP1 |
| `0x23` | `EXP1_SPECTRUM_B` | `PayloadExp1SpectrumB` | EXP1 |
| `0x30` | `EXP2_ENV` | `PayloadExpEnv` | EXP2 |
| `0x31` | `EXP2_STATUS` | `PayloadExp2Status` | EXP2 |
| `0x32` | `EXP2_BER` | `PayloadExp2Ber` | EXP2 |
| `0x40` | `EXP3_ENV` | `PayloadExp3Env` | EXP3 |
| `0x41` | `EXP3_STATUS` | `PayloadExp3Status` | EXP3 |
| `0x42` | `EXP3_STACK_A` | `PayloadExp3StackA` | EXP3 |
| `0x43` | `EXP3_STACK_B` | `PayloadExp3StackB` | EXP3 |
| `0xF0` | `GAP_MARKER` | `PayloadGapMarker` | BTC |
| `0xFE` | `BOOT` | `PayloadBoot` | any |

## BTC

### `BTC_ENV` (0x10) -- 24 B

| Off | Size | Field |
|---|---|---|
| 0 | 1 | `valid_mask` -- b0 ms5611, b1 tmp117, b2 icm42686 |
| 1 | 1 | reserved |
| 2 | 2 | `temp_raw` (TMP117 int16, 1 LSB = 1/128 degC) |
| 4 | 4 | `ms_pressure` (MS5611 D1) |
| 8 | 4 | `ms_temperature` (MS5611 D2) |
| 12 | 6 | `accel_xyz_raw` (ICM-42686, +-32 g, 1024 LSB/g) |
| 18 | 6 | `gyro_xyz_raw` (ICM-42686, +-2000 dps, 16.384 LSB/(deg/s)) |

### `BTC_STATUS` (0x11) -- 12 B, sent at 1 Hz

| Off | Size | Field |
|---|---|---|
| 0 | 4 | `uptime_s` since BTC boot |
| 4 | 4 | `lo_rtc_s` -- RTC seconds-since-midnight when LO was first seen; 0 = not yet |
| 8 | 1 | `sd_mounted_mask` -- b0 mounted, b1 failed |
| 9 | 1 | `signal_mask` -- b0 LO received, b1 SOE active, b2 SODS active |
| 10 | 2 | reserved |

Ground computes absolute UTC as `lo_rtc_s + tick x 0.04`.

## EXP Common

### `EXP_ENV` (0x20, 0x30) -- 12 B

Used by EXP1 and EXP2 (no IMU). Sent every tick.

| Off | Size | Field |
|---|---|---|
| 0 | 1 | `valid_mask` -- b0 ms5611, b1 tmp117 |
| 1 | 1 | reserved |
| 2 | 2 | `temp_raw` |
| 4 | 4 | `ms_pressure` |
| 8 | 4 | `ms_temperature` |

### `EXP_STATUS` (0x21) -- 8 B

Used by EXP1. EXP2 and EXP3 have their own status types.

| Off | Size | Field |
|---|---|---|
| 0 | 4 | `uptime_s` |
| 4 | 1 | `sd_mounted_mask` |
| 5 | 3 | reserved |

## EXP1 Space Disco

### `EXP1_SPECTRUM_A` (0x22) -- 40 B

Channels 1-9 plus measurement metadata. Always paired with
`SPECTRUM_B` for the same measurement.

| Off | Size | Field |
|---|---|---|
| 0 | 36 | `channels[9]` -- uint32 ADC counts; 410/435/460/485/510/535/560/UV-A/UV-B nm |
| 36 | 1 | `integration_cycles` -- total integration = value x 2.8 ms |
| 37 | 1 | `gain` -- 0=1x, 1=3.7x, 2=16x, 3=64x |
| 38 | 1 | `led_mask` -- 0=DARK, 1=RGB, 2=W, 3=IR, 4=UV |
| 39 | 1 | `measurement_valid` -- 1 if DATA_READY was set on collect |

### `EXP1_SPECTRUM_B` (0x23) -- 40 B

| Off | Size | Field |
|---|---|---|
| 0 | 36 | `channels[9]` -- 585/610/645/680/705/730/760/810/NIR nm |
| 36 | 4 | `start_timestamp_us` -- us since LO when the measurement started |

## EXP2 Bouncy Castle

### `EXP2_BER` (0x32) -- 20 B

| Off | Size | Field |
|---|---|---|
| 0 | 1 | `rate_index` -- into the rate table (see [ICD-003](ICD-003-uart-to-lifi-exp2.md)) |
| 1 | 1 | `pattern` -- 0=PRBS-7, 1=PRBS-15, 2=ALL_ONES, 3=ALT_AA |
| 2 | 4 | `timestamp_send_us` |
| 6 | 4 | `timestamp_recv_us` |
| 10 | 2 | `bits_sent` (typically 512) |
| 12 | 2 | `bit_errors` |
| 14 | 1 | `first_error_byte` (`0xFF` = none) |
| 15 | 1 | `last_error_byte` (`0xFF` = none) |
| 16 | 1 | `measurement_valid` |
| 17 | 1 | `sync_check` -- first RX byte, for desync detection |
| 18 | 2 | reserved |

### `EXP2_STATUS` (0x31) -- 12 B

| Off | Size | Field |
|---|---|---|
| 0 | 4 | `uptime_s` |
| 4 | 4 | `watchdog_kicks` |
| 8 | 1 | `sd_mounted_mask` |
| 9 | 3 | reserved |

## EXP3 Floaty Boi

### `EXP3_STACK_A` (0x42) -- 40 B (Wired Stack sample)

| Off | Size | Field |
|---|---|---|
| 0 | 12 | `mag_xyz_raw` (MMC5983MA int32 LE x 3) |
| 12 | 6 | `accel_xyz_raw` (ICM-42688) |
| 18 | 6 | `gyro_xyz_raw` (ICM-42688) |
| 24 | 2 | `tmp_raw` |
| 26 | 4 | `lifi_a_timestamp_us` -- stack-local us since LO |
| 30 | 4 | `latency_a_us` -- wakeup-to-response, stack-measured |
| 34 | 1 | `sample_index` |
| 35 | 1 | `burst_index` |
| 36 | 1 | `valid_mask` -- b0 mag, b1 imu, b2 tmp, b3 cable_check_ok |
| 37 | 3 | reserved |

### `EXP3_STACK_B` (0x43) -- 40 B (Wireless Stack sample)

| Off | Size | Field |
|---|---|---|
| 0 | 12 | `mag_xyz_raw` |
| 12 | 6 | `accel_xyz_raw` |
| 18 | 6 | `gyro_xyz_raw` |
| 24 | 2 | `tmp_raw` |
| 26 | 2 | `cap_voltage_mv` |
| 28 | 4 | `lifi_b_timestamp_us` |
| 32 | 4 | `latency_b_us` |
| 36 | 1 | `sample_index` |
| 37 | 1 | `burst_index` |
| 38 | 1 | `valid_mask` -- b0 mag, b1 imu, b2 tmp, b3 cap_valid |
| 39 | 1 | reserved |

### `EXP3_ENV` (0x40) -- 24 B

| Off | Size | Field |
|---|---|---|
| 0 | 4 | `temp_centidegc` -- TMP117 in 0.01 degC (note: converted, unlike other env payloads) |
| 4 | 4 | `ms_pressure` |
| 8 | 4 | `ms_temperature` |
| 12 | 6 | `imu_accel_xyz_raw` (ICM-42686) |
| 18 | 6 | `imu_gyro_xyz_raw` (ICM-42686) |

The pre-converted temperature is an exception to the raw-data
rule. TODO: revisit at IPR -- either align with the rest (raw) or
document the carve-out more explicitly here.

### `EXP3_STATUS` (0x41) -- 20 B

| Off | Size | Field |
|---|---|---|
| 0 | 4 | `latency_a_estimated_us` -- rolling average |
| 4 | 4 | `latency_b_estimated_us` -- rolling average |
| 8 | 4 | `wait_a_used_us` -- sync delay actually applied this cycle |
| 12 | 2 | `cycle_counter` -- cycles since boot |
| 14 | 1 | `last_burst_sample_count` |
| 15 | 2 | `cycle_period_ms` -- runtime-configurable |
| 17 | 1 | `flags` -- application-defined state bits |
| 18 | 1 | `sensor_disabled_mask` -- permanently disabled sensors this run |
| 19 | 1 | `sd_mounted_mask` -- b0 mounted, b1 failed |

## System

### `GAP_MARKER` (0xF0) -- 4 B (BTC only)

| Off | Size | Field |
|---|---|---|
| 0 | 2 | `first_missing_tick` |
| 2 | 1 | `count` consecutive missing ticks |
| 3 | 1 | `reason` (`GapReason`) |

`GapReason`: `NO_DATA = 0x01`, `CAN_CRC_FAIL = 0x02`,
`LIFI_TIMEOUT = 0x03`, `SENSOR_FAILED = 0x04`.

### `BOOT` (0xFE) -- 4 B

Sent at startup before LO. `tick = 0` and `timestamp_us = 0` in the
header. If it arrives during flight, a watchdog/soft reset
happened and the controller recovered.

| Off | Size | Field |
|---|---|---|
| 0 | 1 | `reason` (`BootReason`) |
| 1 | 1 | reserved |
| 2 | 2 | `reboot_count` -- 0 = first ever boot |

`BootReason`: `COLD_START = 0x01`, `WATCHDOG = 0x02`,
`SOFT_RESET = 0x03`.


## References

- [ADR-009 Raw Sensor Data to Ground](../decisions/ADR-009-raw-sensor-data-to-ground.md)
- [ADR-010 Packet Format](../decisions/ADR-010-packet-format-crc16.md)
- [ICD-006 Packet Format](ICD-006-downlink-packet-format.md)
- [`packet_payloads.hpp`](../../flight-software/shared/comms/packet_payloads.hpp)
- [`packet_types.hpp`](../../flight-software/shared/comms/packet_types.hpp)
