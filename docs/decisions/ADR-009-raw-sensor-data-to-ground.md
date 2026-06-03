# ADR-009: Send Raw Sensor Data to Ground, Compensate on Ground Station

## Status

Accepted

## Date

2026-05-13

## Context

Several sensors on the flight system (notably the MS5611 pressure sensor) 
require non-trivial compensation calculations to convert raw register 
values into physical units. The MS5611 datasheet specifies a multi-step 
second-order compensation algorithm that uses six PROM coefficients and 
includes signed arithmetic with branches based on temperature ranges.

We need to decide where this compensation happens. Two natural locations 
exist:

- On the flight MCU, immediately after reading the sensor
- On the ground station, after receiving the raw values via downlink

The decision affects sensor drivers across the codebase, packet payload 
definitions, and the ground station data pipeline. It also affects what 
the downlinked data looks like for live monitoring during flight.

This decision applies to all sensors where raw-to-physical conversion 
is non-trivial. Currently the MS5611 is the primary case. The TMP117 
and IMU sensors are also candidates because their conversions, while 
simpler, follow the same pattern.

## Decision

We send raw sensor values to the ground station and perform all 
compensation on the ground side.

For the MS5611 specifically, the flight software downlinks the raw 
24-bit D1 (pressure) and D2 (temperature) values. The ground 
station applies the second-order compensation algorithm from the 
datasheet to recover pressure in Pa and temperature in 0.01 degC.

For other sensors, the same principle applies: the flight side reads 
and forwards, the ground side converts.

## Alternatives Considered

### Alternative A: Compensate on the Flight MCU

Run the full compensation algorithm in the sensor driver and downlink 
the converted physical values.

**Rejected because:**

- Adds computational cost to the tick budget for every sensor read. 
  MS5611 second-order compensation is not free, especially the branch 
  logic for the below-20degC path.
- The compensation math involves signed arithmetic that triggers 
  `hicpp-signed-bitwise` warnings. We would need to add yet another 
  documented HIC++ deviation across more code paths.
- If a bug is found in our compensation implementation, we cannot fix 
  it after flight. The raw data is lost.
- The flight MCU has no scientific reason to know the calibrated 
  pressure. Nothing in the flight loop branches on physical values.
- Different sensors would require different conversion logic on each 
  flight MCU, duplicating effort.

### Alternative B: Compensate on the Flight MCU, but Also Send Raw

Compensate on the flight side, but include the raw values in the packet 
for post-flight verification.

**Rejected because:**

- Doubles the payload size for affected sensors, eating into the 
  64-byte RS-422 packet limit.
- Provides the worst of both worlds: we pay the tick cost of compensation 
  and use the downlink bandwidth of raw transmission.
- Encourages confusion about which value is authoritative.

### Alternative C: Compensate on the Flight MCU, Verify on Ground

Compensate on the flight side, but include enough metadata (PROM 
coefficients, intermediate values) to verify the calculation on ground.

**Rejected because:**

- Still has the bug-on-the-rocket problem: if the verification on ground 
  shows the flight calculation was wrong, we cannot recover.
- More complex than the chosen approach with no clear benefit.

## Consequences

### Positive

- Compensation bugs can be fixed after recovery. Raw data is preserved.
- Tick budget on the flight MCU is freed from compensation arithmetic, 
  supporting I-1 (deterministic tick budget).
- HIC++ deviations are reduced. The signed-bitwise math lives on the 
  ground side in Python or similar, where strict embedded coding rules 
  do not apply.
- Sensor drivers are simpler and easier to verify. They read registers 
  and forward bytes, nothing else.
- Different sensor types can be added without revisiting their 
  compensation logic on every flight MCU.
- The ground station can re-process historical data with updated 
  compensation algorithms if better algorithms become available.

### Negative

- Live monitoring during flight requires the ground station to apply 
  compensation in real time. This is a non-issue for our ground software 
  but worth noting.
- The downlink shows raw register values until they reach the ground 
  station, which can be confusing during debugging if someone looks at 
  the raw packet stream directly.
- The BOOT packet must carry sensor calibration data (PROM coefficients 
  for MS5611, similar for other sensors). If the BOOT packet is lost, 
  ground compensation fails until the data is recovered from SD.

### Neutral

- Sensor driver naming becomes "read_raw_d1()" and "read_raw_d2()" rather 
  than "read_pressure_pa()". This matches the intent more accurately.
- The same approach is used in many professional flight systems where 
  the spacecraft is a sensor relay and ground does the science.

## Implementation Notes

The MS5611 driver returns raw `uint32_t` D1 and D2 values. PROM 
coefficients are read once during sensor init and stored. The BOOT 
packet includes the six PROM coefficients per MS5611 instance (BTC, 
EXP1, EXP2, EXP3).

The ground station applies the datasheet compensation including the 
second-order branch (below 20degC vs above). The Python implementation 
mirrors the datasheet algorithm directly without optimisation, because 
ground-side performance is not a concern.

For the IMU and magnetometer sensors, the conversion is a simple 
scaling factor and does not require PROM coefficients. The scaling 
factor is documented in the ICD for each sensor payload, and the ground 
station applies it during decoding.

For temperature from TMP117, the conversion is a single multiply 
(register x 0.0078125 = degC). We still apply this on ground for 
consistency with the other sensors.

## References

- Related ADRs: 
  - [ADR-010 Packet Format with CRC16](ADR-010-packet-format-crc16.md)
- Related ICDs: 
  - [ICD-006 Downlink Packet Format](../interfaces/ICD-006-downlink-packet-format.md)
  - [ICD-007 Packet Payloads](../interfaces/ICD-007-packet-payloads.md)
- System Invariants: I-1 (deterministic tick), I-6 (no silent substitution)
- Datasheet: MS5611-01BA03 Pressure Sensor, Measurement Specialties
- Implementation: 
  - Flight side: `flight-software/shared/sensors/ms5611.cpp`

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-05-15 | @maxatslega | Initial draft |
| 1.0 | 2026-05-15 | @maxatslega | Approved |