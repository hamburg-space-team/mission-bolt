# Documentation

Index of everything written down for THHOR-BOLT REXUS 37.

For the mission overview start at the top-level
[README.md](../README.md), [overview.md](../overview.md), and
[architecture.md](../architecture.md).

## Layout

- **[handbook/](handbook/)** -- how we work as a team
  - [how-we-work.md](handbook/how-we-work.md)
  - [working-groups.md](handbook/working-groups.md)
  - [glossary.md](handbook/glossary.md)
  - [onboarding/](handbook/onboarding/) -- new-joiner sequence
- **[standards/](standards/)** -- the rules
  - [system-invariants.md](standards/system-invariants.md) -- P-1..4, I-1..6
  - [coding/](standards/coding/) -- language-specific
  - [documentation/](standards/documentation/) -- templates
- **[decisions/](decisions/)** -- Architecture Decision Records (ADRs)
- **[interfaces/](interfaces/)** -- Interface Control Documents (ICDs)
- **[guides/](guides/)** -- task-oriented how-tos

## ADRs

| ID | Title |
|---|---|
| [001](decisions/ADR-001-tick-architecture.md) | 40 ms tick loop, no RTOS |
| [002](decisions/ADR-002-class-hierarchy.md) | Class hierarchy for the four flight binaries |
| [003](decisions/ADR-003-littlefs-filesystem.md) | LittleFS on microSD |
| [004](decisions/ADR-004-storage-ringbuffer.md) | Producer-consumer ringbuffer for SD logging |
| [005](decisions/ADR-005-fault-management.md) | Three-strike sensor failure + GAP_MARKER |
| [006](decisions/ADR-006-cmsis-toolbox.md) | CMSIS-Toolbox + CubeMX |
| [007](decisions/ADR-007-iwdg-watchdog-strategy.md) | IWDG refresh once per tick, 600 ms |
| [008](decisions/ADR-008-noinit-ram-recovery.md) | Mid-flight recovery via `.noinit` SRAM |
| [009](decisions/ADR-009-raw-sensor-data-to-ground.md) | Send raw sensor data, compensate on ground |
| [010](decisions/ADR-010-packet-format-crc16.md) | 64-byte packet format with CRC-16/CCITT |

## ICDs

| ID | Title |
|---|---|
| [001](interfaces/ICD-001-rs-422-to-rxsm.md) | RS-422 -- BTC <-> RXSM |
| [002](interfaces/ICD-002-can-protocol.md) | CAN -- BTC <-> experiments |
| [003](interfaces/ICD-003-uart-to-lifi-exp2.md) | UART -- EXP2 <-> LiFi transceivers |
| [004](interfaces/ICD-004-uart-to-lifi-exp3.md) | UART -- EXP3 <-> LiFi + Wired Stack |
| [005](interfaces/ICD-005-i2c-sensor-bus.md) | I2C sensor bus |
| [006](interfaces/ICD-006-downlink-packet-format.md) | Downlink packet format |
| [007](interfaces/ICD-007-packet-payloads.md) | Downlink packet payloads |

## Guides

- [Building the flight software](guides/building-flight-software.md)
- [Debugging with OpenOCD](guides/debugging-with-openocd.md)
- [Running flight unit tests](guides/running-flight-unit-tests.md)
- [Running the ground station](guides/running-ground-station.md)

## Templates

- [ADR template](standards/documentation/adr-template.md)
- [ICD template](standards/documentation/icd-template.md).
