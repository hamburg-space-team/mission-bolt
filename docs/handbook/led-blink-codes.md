# Flight Controller LED Blink Codes

Every board (BTC, EXP1–EXP3) has two status LEDs. This is the operator
reference you read off at the test bench and on the pad.
Implementation: [`status_leds.hpp`](../../flight-software/shared/led/status_leds.hpp).

## CAN LED

| Pattern | Meaning |
|---|---|
| **~3 Hz blink, all boards in step** | CAN SYNC healthy. The phase comes from the bus-wide tick — so if every board blinks together, that's live proof the SYNC distribution is working. If one board drifts out of step, its tick is off. |
| **~0.25 Hz (slow pulse)** | CAN lost — the node keeps ticking on its own (EXPs only; the BTC is the SYNC master and never sees this state). |
| Off | Node isn't running (no tick loop). |

## Error LED: pulse-count codes

No error → LED off. When something goes wrong, the LED blinks out the
code of the source as a group of pulses you can count: *N* short pulses
(120 ms on / 120 ms off each), then an 800 ms pause, then the group
repeats.

If **several** errors are latched, the LED shows each code for **5 s**
and then rotates to the next one. The switch always lands on a group
boundary, so you never catch a group mid-count. Codes stay latched
until reboot (ADR-005: no silent self-healing).

### Code table

Codes 1–5 are the same on every board; 6 and up are board-specific.

| Pulses | Source | Boards | Typical cause |
|---|---|---|---|
| 1 | I²C bus | all | I²C controller init failed — check wiring / pull-ups |
| 2 | MS5611 barometer | all | Sensor not answering / PROM CRC bad |
| 3 | TMP117 temperature | all | Sensor not answering |
| 4 | SD / storage | all | Card missing, mount failed |
| 5 | CAN_BUS | EXPs | CAN TX ring never drains (10 packets dropped in a row) |
| 6 | RS-422 UART downlink | BTC | UART TX ring never drains (10 packets dropped in a row) |
| 7 | ICM-42686 IMU | BTC, EXP3 | Sensor not answering / wrong WHO_AM_I |
| 8 | AS7265X spectrometer | EXP1 | Won't boot (normal until ~1 s after reset), I²C dead |
| 9 | RGB LED driver (LP5810C) | EXP1 | Init/runtime latch after 3 failed recoveries |
| 10 | UV/IR LED driver (LP5810D) | EXP1 | Same as code 9 |

### How to read it

LED blinks: `blink blink blink … (pause) … blink blink blink` → code 3
→ the TMP117 on this board has died. If the pattern flips to 4 pulses
after 5 s, the SD card isn't mounted either.

### Telemetry counterpart

Every error also drops a GAP_MARKER with `reason = SENSOR_FAILED`, and
its `first_missing_tick` field carries the **same code** — so the
ground sees exactly the diagnosis your eye does at the bench.
(Exception: SD errors only show up as an LED code plus the `sd_status`
field in the status packet, since the downlink isn't up yet during
storage init. ICD-007 follow-up note still pending.)