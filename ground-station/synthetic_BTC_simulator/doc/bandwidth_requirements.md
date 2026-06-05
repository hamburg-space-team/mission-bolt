# Bandwidth Requirements & Internal Bus Utilization

The values in this section are an initial estimate based on the current packet sizes and planned cadences. They will be refined during integration testing, where the actual EXP3 burst characteristics, the LiFi-B latency profile, and the SDMMC write throughput can be measured directly. The Min/Normal/Max values below are conservative and provide margin for adjustments.

---

## 1. Per-Source Downlink Rate Estimates

### Table 4.24: Per-source downlink rate estimate at nominal cadence

| Source | Payload | Pkt Size | Cadence | Rate |
| :--- | :--- | :---: | :---: | :---: |
| **BTC** | `BTC_ENV` | 38 B | 25 Hz | 7.6 kbit/s |
| **BTC** | `BTC_STATUS` | 26 B | 1 Hz | 0.2 kbit/s |
| **EXP1** | `EXP1_SPECTRUM_A+_B` | 2 × 54 B | ~7.5 Hz | 6.5 kbit/s |
| **EXP1** | `EXP_ENV` | 26 B | 25 Hz | 5.2 kbit/s |
| **EXP1** | `EXP_STATUS` | 22 B | 1 Hz | 0.2 kbit/s |
| **EXP2** | `EXP2_BER` | 34 B | 25 Hz | 6.8 kbit/s |
| **EXP2** | `EXP_ENV` | 26 B | 25 Hz | 5.2 kbit/s |
| **EXP2** | `EXP2_STATUS` | 26 B | 1 Hz | 0.2 kbit/s |
| **EXP3** | `EXP3_STACK_A` | 54 B | ~50 Hz | 21.6 kbit/s |
| **EXP3** | `EXP3_STACK_B` | 54 B | ~50 Hz | 21.6 kbit/s |
| **EXP3** | `EXP3_ENV` | 38 B | 25 Hz | 7.6 kbit/s |
| **EXP3** | `EXP3_STATUS` | 34 B | 1 Hz | 0.3 kbit/s |
| **Total nominal** | | | | **~83 kbit/s** |

### Per-Source Downlink Allocation
The EXP3 cadence in the table assumes both stacks streaming continuously at 50 Hz per stack, with every sample forwarded to the downlink. The full sample stream is also logged to SD. The exact sample rate is finalised during integration testing once the actual LiFi throughput and the available downlink bandwidth are characterised.

---

## 2. RS-422 Link Bandwidth Requirement Levels

The RS-422 link to the RXSM should support the following bandwidth levels:

### Table 4.26: RS-422 downlink bandwidth requirement levels

| Direction | Min | Normal | Max |
| :--- | :---: | :---: | :---: |
| Downlink (BTC → RXSM) | 50 kbit/s | 83 kbit/s | 115 kbit/s |

* **Minimum (50 kbit/s)**: Covers the reduced-rate downlink needed to demonstrate the minimum success criterion: `BOOT`, `STATUS`, and reduced-cadence science payloads per experiment.
* **Normal (83 kbit/s)**: Corresponds to the per-source table above with all experiments running nominally.
* **Maximum (115 kbit/s)**: Covers worst-case bursts during recovery events (multiple `GAP_MARKER` and `BOOT` packets interleaved with normal science data), or temporarily elevated EXP3 sample rates if the LiFi throughput and downlink budget permit.

---

## 3. Internal Bus Utilization

The internal buses (CAN, UART, I2C, SDMMC) are not subject to the Min/Normal/Max requirement because they are not shared with the rocket. Their nominal utilisation is well below the configured bus speeds:

* **CAN at 1 Mbit/s**: Carries the aggregated downlink traffic plus 25 Hz `SYNC` broadcasts and CAN-frame headers; bus utilisation stays well below capacity.
* **UART at 921,600 Baud**: To LiFi transceivers, carries the per-experiment payload (continuous sample streams on EXP3, buffer-mode transfers on EXP2) well below the configured bus capacity.
* **I2C at 400 kHz**: Carries sensor reads at 25 Hz with small per-transaction payloads; utilisation stays well below capacity.
* **SDMMC at 16 MHz (4-bit)**: Provides ample throughput for the aggregated write traffic per controller.

---
_Document Source Ref: RX37_THHORs-BOLT_SED_v2-0_31May26_
