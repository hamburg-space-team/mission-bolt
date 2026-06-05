# THHOR-BOLT REXUS 37 Downlink Packet Protocol Specification

This document details the downlink packet protocol, header layout, payload structures, and the bxCAN transport layer fragmentation format used in the THHOR-BOLT flight software.

---

## 1. High-Level Packet Wire Format

Each telemetry packet transmitted by the flight software has a **maximum size of 64 bytes**. It consists of three main parts laid out contiguously in memory:

```
+-------------------+-------------------------------+-------------------+
|  Header (12 B)    |     Payload (0 to 50 B)       |    CRC-16 (2 B)   |
+-------------------+-------------------------------+-------------------+
```

---

## 2. Packet Header Layout

The `PacketHeader` structure is exactly **12 bytes** wide and contains synchronization, routing, sequencing, and timing metadata.

### Byte-by-Byte Layout

| Byte Offset | Field | Data Type | Value / Description |
| :---: | :--- | :--- | :--- |
| `0` | `sync[0]` | `uint8_t` | Constant synchronization byte: `0xB0` (`SYNC_0`) |
| `1` | `sync[1]` | `uint8_t` | Constant synchronization byte: `0x17` (`SYNC_1`) |
| `2` | `version` | `uint8_t` | Protocol version: `0x01` (`PROTOCOL_VERSION`) |
| `3` | `type` | `uint8_t` | Payload type identifier (corresponds to `PayloadType` enum) |
| `4` | `sequence` | `uint8_t` | A per-type rolling counter. Wraps at 255 |
| `5` | `length` | `uint8_t` | Length of the payload in bytes (0 to 50) |
| `6 - 7` | `tick` | `uint16_t` | 25 Hz clock ticks since Lift-Off (LO). Little-Endian |
| `8 - 11` | `timestamp_us` | `uint32_t` | Microseconds elapsed since Lift-Off (LO). Little-Endian |

---

## 3. CRC-16 Checksum

* **Polynomial**: Calculated using a standard CRC-16 algorithm over the packet.
* **Scope**: Calculated over all bytes **starting after the two synchronization bytes** (i.e. from the `version` byte at offset 2 up to the last byte of the payload).
* **Format**: Placed at the very end of the packet as 2 bytes in **Big-Endian** order:
  * `buf[HEADER_SIZE + payload_len]` = `(crc >> 8) & 0xFF`
  * `buf[HEADER_SIZE + payload_len + 1]` = `crc & 0xFF`

---

## 4. Payload Types & Structures

Payloads are standard layout, trivially copyable, and packed structures. They are categorized into system diagnostic payloads and experimental scientific payloads.

### 4.1 System & Diagnostic Payloads

#### Boot Notification (`PayloadBoot` — Type: `0xFE`)
Sent at power-on before LO. In the packet header, `tick` and `timestamp_us` are set to `0`. If received during flight, it indicates a watchdog or software reset recovery.
* **Reason (`uint8_t`)**: Startup cause.
  * `0x01` = `COLD_START` (Normal power-on)
  * `0x02` = `WATCHDOG` (Watchdog fired, main loop hung)
  * `0x03` = `SOFT_RESET` (Intentional software reset)
* **Reserved (`uint8_t`)**: 1 byte.
* **Reboot Count (`uint16_t`)**: Total boots since first power-on (Little-Endian).

#### Gap Marker (`PayloadGapMarker` — Type: `0xF0`)
Emitted by the BTC to explicitly flag missed ticks or communication dropouts.
* **First Missing Tick (`uint16_t`)**: Tick number of the start of the gap.
* **Count (`uint8_t`)**: Number of consecutive missing ticks.
* **Reason (`uint8_t`)**: Gap cause:
  * `0x01` = `SENSOR_FAILED` (Sensor non-functional; EXP sends this to BTC over CAN)
  * `0x02` = `SCHEDULER_OVERRUN` (BTC main loop ran late; BTC sends to RS-422 downlink)
  * `0x03` = `SYNC_MISSED` (EXP did not receive expected BTC SYNC frames)

---

### 4.2 Board Terminal Controller (BTC) Payloads

#### BTC Environment (`PayloadBtcEnv` — Type: `0x10`)
* **Valid Mask (`uint8_t`)**:
  * Bit 0: MS5611 valid
  * Bit 1: TMP117 valid
  * Bit 2: ICM-42686 valid
* **Reserved (`uint8_t`)**: 1 byte.
* **TMP117 Temperature (`int16_t`)**: Raw register value ($1\text{ LSB} = 1/128\ ^\circ\text{C}$, signed).
* **MS5611 Pressure (`uint32_t`)**: Raw D1 pressure value.
* **MS5611 Temperature (`uint32_t`)**: Raw D2 temperature value.
* **ICM-42686-P Accelerometer (`int16_t[3]`)**: Raw 16-bit register values for X, Y, and Z.
* **ICM-42686-P Gyroscope (`int16_t[3]`)**: Raw 16-bit register values for X, Y, and Z.

#### BTC Status (`PayloadBtcStatus` — Type: `0x11`)
Sent every 25 ticks (1 Hz).
* **Uptime (`uint32_t`)**: Seconds since MCU boot (not since LO).
* **LO RTC Timestamp (`uint32_t`)**: RTC seconds-since-midnight when LO was detected. 0 if LO not yet received.
* **SD Card Status (`uint8_t`)**:
  * Bit 0: Mounted
  * Bit 1: Failed (3+ consecutive write errors, writes suppressed)
* **REXUS Signals (`uint8_t`)**:
  * Bit 0: LO received (latched)
  * Bit 1: SOE currently active
  * Bit 2: SODS currently active
* **Reserved (`uint8_t[2]`)**: 2 bytes.

---

### 4.3 Experiment 1: Space Disco

> [!NOTE]
> Since the AS7265X spectrometer has 18 channels, it exceeds the 50-byte payload limit. The science data is therefore split into two payloads (`SPECTRUM_A` and `SPECTRUM_B`) and transmitted in the same tick.

#### EXP1 Spectrum A (`PayloadExp1SpectrumA` — Type: `0x20`)
* **Channels 1–9 (`uint32_t[9]`)**: Raw readings for wavelengths 410nm, 435nm, 460nm, 485nm, 510nm, 535nm, 560nm, UV-A, and UV-B.
* **Integration Cycles (`uint8_t`)**: AS7265X register value indicating integration duration.
* **Gain (`uint8_t`)**: Selected sensor gain ($0 = 1\times$, $1 = 3.7\times$, $2 = 16\times$, $3 = 64\times$).
* **LED Mask (`uint8_t`)**: LED illumination: $0 = \text{dark}$, $1 = \text{RGB}$, $2 = \text{white}$, $3 = \text{IR } (940\text{ nm})$, $4 = \text{UV } (400\text{ nm})$.
* **Measurement Valid (`uint8_t`)**: `1` if `DATA_READY` was set when results were collected; `0` if incomplete.

#### EXP1 Spectrum B (`PayloadExp1SpectrumB` — Type: `0x21`)
* **Channels 10–18 (`uint32_t[9]`)**: Raw readings for wavelengths 585nm, 610nm, 645nm, 680nm, 705nm, 730nm, 760nm, 810nm, and NIR.
* **Start Timestamp (`uint32_t`)**: Timestamp when the measurement started ($\mu\text{s}$ since LO).

#### Shared Experiment Environment (`PayloadExpEnv` — Types: `EXP1_ENV (0x22)`, `EXP2_ENV (0x32)`)
* **Valid Mask (`uint8_t`)**: Bit 0 = MS5611 valid, Bit 1 = TMP117 valid.
* **Reserved (`uint8_t`)**: 1 byte.
* **TMP117 Temperature (`int16_t`)**: Raw register value ($1\text{ LSB} = 1/128\ ^\circ\text{C}$).
* **MS5611 Pressure (`uint32_t`)**: Raw pressure.
* **MS5611 Temperature (`uint32_t`)**: Raw temperature.

#### Shared Experiment Status (`PayloadExpStatus` — Types: `EXP1_STATUS (0x23)`, `EXP2_STATUS (0x31)`)
* **Uptime (`uint32_t`)**: Seconds since MCU boot.
* **SD Card Status (`uint8_t`)**: Bit 0 = mounted, Bit 1 = failed.
* **Reserved (`uint8_t[3]`)**: 3 bytes.

---

### 4.4 Experiment 2: Bouncy Castle

#### EXP2 BER (`PayloadExp2Ber` — Type: `0x30`)
LiFi bit error rate measurements for a single transmission round. Sent every tick.
* **Rate Index (`uint8_t`)**: Index into the LiFi transmission rate lookup table.
* **Send Timestamp (`uint32_t`)**: Timestamp when the send command was executed ($\mu\text{s}$ since LO).
* **Receive Timestamp (`uint32_t`)**: Timestamp when the response buffer was fully received ($\mu\text{s}$ since LO).
* **Bits Transmitted (`uint16_t`)**: Total count of bits transmitted in this round.
* **Bit Errors (`uint16_t`)**: Number of mismatched bits detected.
* **First Error Byte (`uint8_t`)**: Byte index of first error. `0xFF` if error-free.
* **Last Error Byte (`uint8_t`)**: Byte index of last error. `0xFF` if error-free.
* **Measurement Valid (`uint8_t`)**: `1` if valid; `0` if invalid.

---

### 4.5 Experiment 3: Floaty Boi

#### EXP3 Stack A (`PayloadExp3StackA` — Type: `0x40`)
One sample from the **wired** stack (STM32U0 + Molex cable + LiFi A).
* **MMC5983MA Magnetometer (`int32_t[3]`)**: Raw 18-bit register values (sign-extended) for X, Y, and Z.
* **ICM-42688-P Accelerometer (`int16_t[3]`)**: Raw 16-bit register values for X, Y, and Z.
* **ICM-42688-P Gyroscope (`int16_t[3]`)**: Raw 16-bit register values for X, Y, and Z.
* **TMP117 Temperature (`int16_t`)**: Raw register value ($1\text{ LSB} = 1/128\ ^\circ\text{C}$).
* **LiFi A Timestamp (`uint32_t`)**: Time when the wired stack MCU latched this sample ($\mu\text{s}$ since LO).
* **Latency A (`uint32_t`)**: STM32-measured wakeup-to-response latency ($\mu\text{s}$).
* **Burst Index (`uint8_t`)**: Sample index within the current burst sequence.
* **Valid Mask (`uint8_t`)**:
  * Bit 0: Magnetometer valid
  * Bit 1: IMU valid
  * Bit 2: Temperature sensor valid
  * Bit 3: Cable handshake passed (`cable_check_ok`)
* **Reserved (`uint8_t[3]`)**: 3 bytes.

#### EXP3 Stack B (`PayloadExp3StackB` — Type: `0x41`)
One sample from the **wireless** stack (STM32U0 + laser power + LiFi B).
* **MMC5983MA Magnetometer (`int32_t[3]`)**: Raw 18-bit register values for X, Y, and Z.
* **ICM-42688-P Accelerometer (`int16_t[3]`)**: Raw 16-bit register values for X, Y, and Z.
* **ICM-42688-P Gyroscope (`int16_t[3]`)**: Raw 16-bit register values for X, Y, and Z.
* **TMP117 Temperature (`int16_t`)**: Raw register value ($1\text{ LSB} = 1/128\ ^\circ\text{C}$).
* **Storage Capacitor Voltage (`uint16_t`)**: Wireless-stack capacitor voltage level.
* **LiFi B Timestamp (`uint32_t`)**: Time when the wireless stack MCU latched this sample ($\mu\text{s}$ since LO).
* **Latency B (`uint32_t`)**: STM32-measured wakeup-to-response latency ($\mu\text{s}$).
* **Burst Index (`uint8_t`)**: Sample index within the current burst sequence.
* **Valid Mask (`uint8_t`)**:
  * Bit 0: Magnetometer valid
  * Bit 1: IMU valid
  * Bit 2: Temperature sensor valid
  * Bit 3: Capacitor voltage sensor valid
* **Reserved (`uint8_t[2]`)**: 2 bytes.

#### EXP3 Environment (`PayloadExp3Env` — Type: `0x42`)
* **TMP117 Temperature (`int16_t`)**: Raw register value ($1\text{ LSB} = 1/128\ ^\circ\text{C}$).
* **MS5611 Pressure (`uint32_t`)**: Raw MS5611 pressure.
* **MS5611 Temperature (`uint32_t`)**: Raw MS5611 temperature.
* **ICM-42686-P Accelerometer (`int16_t[3]`)**: Raw accelerometer X, Y, Z.
* **ICM-42686-P Gyroscope (`int16_t[3]`)**: Raw gyroscope X, Y, Z.

#### EXP3 Status (`PayloadExp3Status` — Type: `0x43`)
* **Average Latency A (`uint32_t`)**: Rolling average latency of the wired stack ($\mu\text{s}$).
* **Average Latency B (`uint32_t`)**: Rolling average latency of the wireless stack ($\mu\text{s}$).
* **Wait Delay Used (`uint32_t`)**: Sync delay applied at the start of the current cycle ($\mu\text{s}$).
* **SD Card Status (`uint8_t`)**: Bit 0 = mounted, Bit 1 = failed.

---

## 5. bxCAN Transport Layer Fragmentation

To transmit packets of up to 64 bytes over standard CAN (which limits payload to 8 bytes per frame), the `BxcanTransport` class implements a fragmentation scheme.

### CAN Frame Structure

Each physical CAN frame carries an **8-byte payload**, structured as:

| Byte | Field | Description |
| :---: | :--- | :--- |
| `0` | **Fragmentation Header** | Split into two 4-bit nibbles: `(frame_index << 4) \| (frame_count & 0x0F)` |
| `1 - 7` | **Packet Data Chunk** | 7-byte segment of the reassembled packet |

### Fragmentation Header Layout (Byte 0)

* **`frame_count` (Lower 4 bits)**: Total number of frames in the fragmented packet.
  $$\text{frame\_count} = \left\lceil \frac{\text{packet\_length}}{7} \right\rceil$$
* **`frame_index` (Upper 4 bits)**: Zero-indexed position of the current frame (from `0` to `frame_count - 1`).

### Properties & Edge Cases

* **Maximum Size**: Supports up to 15 frames ($15 \times 7 = 105$ bytes), easily accommodating the 64-byte packet limit.
* **Padding**: The final frame is zero-padded if the remaining packet data is less than 7 bytes.
* **Mailbox Flow Control**:
  * Before adding a new frame, the transport checks `HAL_CAN_GetTxMailboxesFreeLevel`.
  * If no mailbox is free, it polls for a mailbox slot with a timeout of **2 ms**. If the timeout is exceeded, the remainder of the packet transmission is aborted to prevent blocking the flight loop.
