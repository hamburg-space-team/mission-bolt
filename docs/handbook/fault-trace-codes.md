# Fault Trace Codes (FAULT packet, type 0xF1)

Every latched fault (sensor init failure, runtime device latch, dead TX
ring) is reported once as a **FAULT packet** carrying the error's step
trace: *what* failed, *where in the call chain* it originated, the
*source line* of the origin and the *microsecond timestamp of
occurrence*. This replaces the old `SENSOR_FAILED` gap-marker
convention (fault code in `first_missing_tick`), which is no longer
emitted. Design: ADR-012. Implementation:
[`errors.hpp`](../../flight-software/shared/utils/errors.hpp).

## Packet layout

Standard packet header, then 12 payload bytes:

| Field | Size | Meaning |
|---|---|---|
| `fault_code` | 1 | Fault source = **LED blink code** (see [led-blink-codes.md](led-blink-codes.md)) |
| `error_code` | 1 | Cause, see ErrorCode table below |
| `flags` | 1 | bit 0 = trace truncated (real chain deeper than 6) |
| `depth` | 1 | Valid entries in `steps[]` |
| `line` | 2 | Source line of the origin (`fail()` call site) — only meaningful with the exact firmware build |
| `steps[6]` | 6 | Step dictionary values, `steps[0]` = origin, then outward |

Header semantics differ from data packets: **`timestamp_us` = moment
the error occurred at its origin** (stamped inside `fail()`), **`tick`
= CAN tick current when the fault was reported** (0 during init).
Synthesized faults (TX-ring latches) have `line = 0`.

## How to read a trace

Read `steps` left to right = innermost to outermost:

```
fault_code = 8 (SPEC), error_code = 2 (TIMEOUT), depth = 3
steps = [0x15, 0x45, 0x47]
       -> I2C_WAIT_COMPLETE -> SPEC_VREG_READ -> SPEC_READ_DIES
```

"While reading the spectrometer dies, a virtual-register read timed
out waiting for the I²C transfer to complete" — plus the exact origin
line and the microsecond it happened.

## ErrorCode table

| Value | Name | Meaning |
|---|---|---|
| 1 | BUS_ERROR | I/O fault on a bus transaction |
| 2 | TIMEOUT | Deadline missed |
| 3 | BAD_ARGUMENT | Caller-side mistake (null pointer, range) |
| 4 | DISABLED | Device latched as failed / never initialised |
| 5 | PROTOCOL_ERROR | Wire-level mismatch (NACK, bad ID, bad CRC) |
| 6 | IO_ERROR | Storage backend failed |
| 7 | OUTPUT_TOO_LARGE | Output buffer too small |

## Step dictionary

Values are **wire-stable and append-only**: never renumber or reuse,
only add new entries inside a module's range. One entry per *semantic
level* of a driver operation, not per bus transaction. Source of truth
is the `Step` enum in
[`errors.hpp`](../../flight-software/shared/utils/errors.hpp).

| Value | Name | Module / meaning |
|---|---|---|
| 0x00 | NONE | unused trace slot |
| **0x1x** | | **I²C bus (CmsisI2CBus)** |
| 0x10 | I2C_INIT | init: Initialize/PowerControl |
| 0x11 | I2C_RESET | full peripheral reset |
| 0x12 | I2C_WRITE | write(): MasterTransmit + completion |
| 0x13 | I2C_READ | read(): MasterReceive + completion |
| 0x14 | I2C_WRITE_READ | write_read(): TX phase (RX phase = I2C_READ) |
| 0x15 | I2C_WAIT_COMPLETE | completion wait: timeout / NACK / bus error |
| **0x2x** | | **MS5611 barometer** |
| 0x20 | BARO_RESET | reset command |
| 0x21 | BARO_PROM_READ | PROM coefficient readout |
| 0x22 | BARO_PROM_CRC | PROM CRC mismatch |
| 0x23 | BARO_START_CONV | D1/D2 conversion start |
| 0x24 | BARO_READ_ADC | ADC result readout |
| 0x25 | BARO_COLLECT | ADC = 0, conversion unfinished |
| 0x26 | BARO_READ | read(): DISABLED / pipeline priming |
| **0x3x** | | **TMP117 temperature** |
| 0x30 | TMP_INIT | device-ID readout |
| 0x31 | TMP_ID_CHECK | DEV_ID mismatch |
| 0x32 | TMP_CONFIG | continuous-mode config write |
| 0x33 | TMP_READ | temperature readout |
| **0x4x** | | **AS7265X spectrometer** |
| 0x40 | SPEC_INIT | init: integration time + control setup |
| 0x41 | SPEC_START_MEAS | one-shot measurement start |
| 0x42 | SPEC_SET_INTEGRATION | integration-time change |
| 0x43 | SPEC_WAIT_STATUS | mailbox TX/RX status poll |
| 0x44 | SPEC_VREG_WRITE | virtual-register write |
| 0x45 | SPEC_VREG_READ | virtual-register read |
| 0x46 | SPEC_DEV_SEL | die select (VREG_DEV_SEL) |
| 0x47 | SPEC_READ_DIES | channel readout |
| **0x5x** | | **LP5810 LED drivers** |
| 0x50 | LED_INIT | init() |
| 0x51 | LED_CONFIGURE | power-on register sequence |
| 0x52 | LED_ENABLE_CHIP | Chip_EN retries exhausted |
| 0x53 | LED_SET_CHANNELS | set_channels incl. failed recovery |
| 0x54 | LED_APPLY | PWM0-3 + LED_EN writes |
| 0x55 | LED_DISABLE_ALL | disable_all() |
| 0x56 | LED_RECOVER | bus reset + re-init + re-apply |
| **0x6x** | | **ICM42686 / ICM42688 IMU** |
| 0x60 | IMU_WHOAMI | WHO_AM_I readout / mismatch |
| 0x61 | IMU_CONFIG_ACCEL | accel config write |
| 0x62 | IMU_CONFIG_GYRO | gyro config write |
| 0x63 | IMU_POWER_ON | PWR_MGMT0 write |
| 0x64 | IMU_READ | burst data readout |
| 0x65 | IMU_CONFIG_INT | INT1 routing/config writes (data-ready) |
| **0x7x** | | **Storage (SdStore)** |
| 0x70 | SD_INIT | card info readout |
| 0x71 | SD_MOUNT | littlefs mount/format |
| 0x72 | SD_OPEN | log file open |
| 0x73 | SD_WRITE | ring-buffer producer (incl. ring full) |
| 0x74 | SD_FLUSH | lfs_file_sync |
| **0x8x** | | **Comms** |
| 0x80 | PKT_BUILD | packet payload too large |
| 0x81 | CAN_TX_RING | CAN TX frame ring latched (never drains) |
| 0x82 | UART_TX_RING | RS-422 TX byte ring latched (never drains) |

## Reporting rules

- Every fault source reports **once per boot** (one-shot guards /
  `poll_device_fault()`); the LED code keeps blinking until reboot
  (ADR-005).
- Runtime device latches report the **death trace** captured at the
  moment of latching (`DeviceBase::last_error()`), even when the
  report happens a tick later — timestamp and line stay exact.
- SD faults are LED + `sd_status` only (no FAULT packet: the links are
  not up during storage init and the SD itself is the failed part).
- CAN/UART ring faults still emit the packet: the dead link drops it,
  but it lands in the SD log.
