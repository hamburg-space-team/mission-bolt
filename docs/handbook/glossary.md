# Glossary

Terms and acronyms used across THHOR-BOLT documentation. When in
doubt, check here first.

For deeper-than-a-definition treatment, follow the linked
documents.

## Mission and Organisation

| Term | Definition |
|------|------------|
| **THHOR** | TUHH Orbital Research |
| **HST** | Hamburg Space Team, the umbrella student organisation |
| **THHOR-BOLT** | This project: Beamed Optical Light Transmission demonstrator on REXUS 37 |
| **REXUS** | Rocket EXperiments for University Students; suborbital sounding-rocket programme operated by DLR and SSC |
| **REXUS 37** | The specific REXUS flight THHOR-BOLT is manifested on, planned for Q1 2027 from Esrange Space Center, Sweden |
| **THHOR-HANS** | A follow-on satellite mission that consumes BOLT's results |
| **REXUS User Manual** | The operational and interface specification document maintained by the REXUS programme |

## Review Milestones

| Term | Definition |
|------|------------|
| **SDR** | Student Application Document, the entry-level proposal review |
| **PDR** | Preliminary Design Review |
| **CDR** | Critical Design Review |
| **IPR** | Integration Progress Report |
| **EAR** | Experiment Acceptance Review |

## Subsystems

| Term | Definition |
|------|------------|
| **BTC** | Bifröst Test Controller. STM32L476-class master controller and downlink gateway |
| **EXP1** | "Space Disco" -- spectral characterisation experiment |
| **EXP2** | "Bouncy Castle" -- bit-error-rate experiment with floating particles |
| **EXP3** | "Floaty Boi" -- wireless power + LiFi demonstration with magnetometer comparison |
| **RXI** | REXUS Interface; the bulkhead and structural interface to the rocket |
| **RXSM** | REXUS Service Module; provides power, discrete signals, and the PCM downlink path |
| **Wired Stack** | EXP3 sensor PCB powered and signalled over cable plus LiFi A |
| **Wireless Stack** | EXP3 sensor PCB powered through a solar cell + supercapacitor, signalled over LiFi B |

## Signals and Time

| Term | Definition |
|------|------------|
| **LO** | Lift-Off discrete signal from RXSM; defines `t = 0` for the science timeline |
| **SODS** | Start of Data Storage discrete signal from RXSM |
| **SOE** | Start of Experiment discrete signal from RXSM |
| **T-, T+** | Time before (T-) or after (T+) the LO event |
| **tick** | The 25 Hz, 40 ms cycle that drives every flight controller |
| **science window** | The ~600 s interval around apogee where useful microgravity science is collected |

## Software Architecture

| Term | Definition |
|------|------------|
| **FlightComputer** | Abstract base class that owns the 40 ms tick loop and `PacketBuilder` |
| **NodeComputer** | Intermediate base class that adds the sensors and SD shared by every node |
| **BtcComputer** | Concrete class running on the BTC |
| **ExpComputer** | Abstract CAN-slave framework shared by Exp1/2/3 |
| **Platform** | Function-pointer abstraction over delay/tick/watchdog/LED primitives, makes host testing possible |
| **on_tick()** | Pure virtual hook called once per 40 ms; implements per-controller behaviour |
| **on_init()** | One-shot initialiser called before the tick loop |
| **PacketBuilder** | Producer that writes a downlink packet into a caller-owned buffer |
| **GAP_MARKER** | Special downlink packet that flags a known on-board failure or missed slot |
| **BOOT packet** | Sent at startup, carries reset reason and reboot count |

## Design Principles and Invariants

| Term | Definition |
|------|------------|
| **P-1 ... P-4** | The four design principles in [docs/standards/system-invariants.md](../standards/system-invariants.md) |
| **I-1 ... I-7** | The seven testable invariants that operationalise the principles |
| **Three-strike rule** | A sensor is permanently disabled after 3 consecutive read failures |
| **Autonomous mode** | An experiment controller's fallback after 5 missed CAN SYNC frames (200 ms) |

## Buses and Protocols

| Term | Definition |
|------|------------|
| **CAN** | Controller Area Network; BTC <-> experiments at 1 Mbit/s ([ICD-002](../interfaces/ICD-002-can-protocol.md)) |
| **RS-422** | The differential serial link from BTC to RXSM ([ICD-001](../interfaces/ICD-001-rs-422-to-rxsm.md)) |
| **I2C / I2C** | Inter-Integrated Circuit; Fast Mode (400 kHz) sensor bus ([ICD-005](../interfaces/ICD-005-i2c-sensor-bus.md)) |
| **UART** | Universal Asynchronous Receiver/Transmitter; used for LiFi transceiver links |
| **SDMMC** | STM32 peripheral that drives the microSD card in 4-bit mode at 16 MHz |
| **SYNC packet** | CAN broadcast at 25 Hz from BTC carrying the current tick value |

## Storage

| Term | Definition |
|------|------------|
| **LittleFS** | Power-fail-safe filesystem used on every microSD ([ADR-003](../decisions/ADR-003-littlefs-filesystem.md)) |
| **log.bin** | The per-controller append-only LittleFS log file |
| **Ringbuffer** | Producer-consumer RAM buffer between `on_tick()` and `SdStore` ([ADR-004](../decisions/ADR-004-storage-ringbuffer.md)) |
| **.noinit** | Linker section in SRAM that survives software resets ([ADR-008](../decisions/ADR-008-noinit-ram-recovery.md)) |

## Sensors and Hardware

| Term | Definition |
|------|------------|
| **MS5611** | Pressure + temperature sensor, I2C `0x77` |
| **TMP117** | Precision temperature sensor, I2C `0x48` |
| **ICM-42686-P** | +-32 g / +-2000 dps IMU on BTC and EXP3 controllers |
| **ICM-42688-P** | +-32 g / +-2000 dps IMU on EXP3 stacks (different part, similar interface) |
| **AS7265X** | 18-channel spectrometer on EXP1, I2C `0x49` |
| **MMC5983MA** | Magnetometer on the EXP3 stacks |
| **LP5810** | LED driver IC, two instances on EXP1 (C at `0x14`, D at `0x15`) |
| **STM32L476RGT6** | Main MCU on BTC, EXP1, EXP2, EXP3 |
| **STM32U031F8** | Small MCU on the LiFi transceivers and EXP3 stacks |
| **VBPW34S** | Photodiode used on the LiFi transceiver receive side |

## Verification

| Term | Definition |
|------|------------|
| **HIL** | Hardware-in-the-Loop test platform, see [bolt-thhor-hil](https://github.com/hamburg-space-team/bolt-thhor-hil) |
| **clang-tidy** | Static analyser; runs on every PR and targets HIC++ compliance |
| **clang-format** | Code formatter; the team-standard style |
| **Catch2** | Host-side unit test framework |
| **HIC++** | High Integrity C++; the coding standard we adapt for flight code |

## Ground Software

| Term | Definition |
|------|------------|
| **sensor-api** | The bridge that decodes packets and serves SSE to the dashboard |
| **sensor-dashboard** | The React + Vite frontend used by operators |
| **SSE** | Server-Sent Events; how live packets reach the dashboard |
| **PCM** | Pulse Code Modulation; the REXUS-side RF chain that carries our downlink |

## Documents

| Term | Definition |
|------|------------|
| **ADR** | Architecture Decision Record; one decision, one document, one PR |
| **ICD** | Interface Control Document; the contract between two parties |
| **CDR document** | The CDR LaTeX source under [`docs/cdr/`](../cdr/) |
