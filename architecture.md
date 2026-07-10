# THHOR-BOLT REXUS 37 -- Software Architecture

This document gives a high-level overview of the flight and ground software
architecture for the REXUS 37 mission. For detailed technical descriptions,
follow the references at the end of each section.

For mission goals and scientific context, see [overview.md](overview.md).

## System Overview

The flight software runs on four STM32 microcontrollers in a master-slave hierarchy:

```markdown
+-------------------------------------------------------------+
|                    REXUS Service Module                      |
+--------------------------+----------------------------------+
                           | RS-422 + LO, SODS, SOE signals
                  +--------v---------+
                  |   BTC            |
                  |   STM32F756ZG    |
                  +-----+------+-----+
                        | CAN 1 Mbit/s, 25 Hz SYNC
            +-----------+--------------+
            |           |              |
       +----v---+  +----v---+   +-----v---+
       | EXP1   |  | EXP2   |   | EXP3    |
       | L476   |  | L476   |   | L476    |
       +--------+  +--------+   +---------+
        Space       Bouncy        Floaty
        Disco       Castle        Boi
```

Six STM32U031F8 microcontrollers serve as LiFi transceivers: two for EXP2,
two for EXP3 main board, and two for the EXP3 sensor stacks.

The Bifröst Test Controller (BTC) coordinates all experiments, distributes
timing via CAN, and forwards telemetry to the REXUS Service Module. Each
experiment controller runs autonomously with the BTC providing
synchronisation reference.

## Design Principles and Invariants

Every architectural decision follows from the single-flight
constraint: once airborne, we cannot push fixes. Four design
principles (P-1..P-4) and seven testable invariants (I-1..I-7)
drive the rest of this document.

See [docs/standards/system-invariants.md](docs/standards/system-invariants.md).
ADRs reference these by number; this document does too.

## Flight Software Architecture

### Tick-Based Execution

Each flight computer runs a deterministic 40 ms tick loop without an RTOS.
This design provides bounded execution time, predictable scheduling, and
analysability by inspection.

See [ADR-001 Tick Architecture](docs/decisions/ADR-001-tick-architecture.md).

### Class Hierarchy

The flight code is organised as a layered class hierarchy. The same source
tree compiles into four distinct flight binaries.

```markdown
FlightComputer (abstract) -- tick loop, lifecycle hooks
    |
    +--> NodeComputer (abstract) -- sensors, storage, status LEDs
            |
            +--> BtcComputer -- CAN master, RS-422 downlink
            |
            +--> ExpComputer (abstract) -- CAN slave framework
                    |
                    +--> Exp1Computer -- Space Disco
                    +--> Exp2Computer -- Bouncy Castle
                    +--> Exp3Computer -- Floaty Boi
```

Each board-specific class adds only 30 to 80 lines on top of the shared
base. The tick scheduler, packet framework, CAN synchronisation, sensor
initialisation, and SD persistence live in the shared layer.

See [ADR-002 Flight Software Class Hierarchy](docs/decisions/ADR-002-class-hierarchy.md).

### Storage

Each controller uses LittleFS on microSD with a producer-consumer
ringbuffer to decouple SD write latency from the tick loop. Explicit sync
runs every 25 ticks (1 Hz) and on critical events.

See [ADR-003 LittleFS](docs/decisions/ADR-003-littlefs-filesystem.md)
and [ADR-004 Storage Ringbuffer](docs/decisions/ADR-004-storage-ringbuffer.md).

### Fault Management

Sensor failures are detected with a threshold of 3 consecutive errors.
Mid-flight resets recover state from `.noinit` SRAM. Experiment controllers
enter autonomous mode if BTC SYNC is missed for 200 ms.

See [ADR-005 Fault Management](docs/decisions/ADR-005-fault-management.md)
and [ADR-008 .noinit Recovery](docs/decisions/ADR-008-noinit-ram-recovery.md).

## Communication Architecture

| Interface | Purpose | Specification |
| ----------- | --------- | --------------- |
| RS-422 115200 baud | BTC -> RXSM (downlink) | [ICD-001](docs/interfaces/ICD-001-rs-422-to-rxsm.md) |
| CAN 1 Mbit/s | BTC <-> Experiments, 25 Hz SYNC | [ICD-002](docs/interfaces/ICD-002-can-protocol.md) |
| UART 921600 baud | EXP2 <-> LiFi transceivers | [ICD-003](docs/interfaces/ICD-003-uart-to-lifi-exp2.md) |
| UART 921600 baud | EXP3 <-> LiFi + Wired Stack | [ICD-004](docs/interfaces/ICD-004-uart-to-lifi-exp3.md) |
| I2C 400 kHz | Sensor buses (per controller) | [ICD-005](docs/interfaces/ICD-005-i2c-sensor-bus.md) |

All interfaces use hard timeouts on the critical path per I-2.

## Per-Experiment Software

Each experiment has distinct software characteristics:

**EXP1 Space Disco** runs a pipelined state machine cycling through LED
configurations and spectrometer integration times. Each measurement spans
multiple ticks.

**EXP2 Bouncy Castle** measures one BER point per tick using a pipelined
pattern: read previous result, compute BER, trigger next transmission.

**EXP3 Floaty Boi** operates on a 3.1-second duty cycle driven by LED
charging: Active phase (3 ticks), Finalize phase (1 tick), Charging
phase (~74 ticks).

## Toolchain

- **Language:** C++20 (`-std=gnu++20`)
- **Compiler:** ARM GNU Toolchain `arm-none-eabi-gcc` 13.3.1
- **Build System:** CMSIS-Toolbox 2.12.0
- **Configuration:** STM32CubeMX for HAL/peripheral setup
- **Filesystem:** LittleFS v2.11.3
- **Test Framework:** Catch2 v3.7.1
- **CI/CD:** GitHub Actions

The flight code does not use the C++ standard library beyond `<cstdint>`,
`<cstddef>`, and `<array>`. Heap-allocating containers are not used.

See [ADR-006 CMSIS-Toolbox](docs/decisions/ADR-006-cmsis-toolbox.md).

## Code Quality

Static analysis with `clang-tidy` targeting HIC++ compliance runs on every
pull request. Four documented deviations cover GCC extensions required
for binary protocols, vendor inline assembly, packed struct layout, and
datasheet-prescribed signed arithmetic.

See [docs/standards/coding/cpp.md](docs/standards/coding/cpp.md).

## Verification

Verification combines static analysis, host-side unit tests, and
target-level testing on the HIL platform. Unit tests cover safety-critical
modules: CRC16, PacketBuilder, BootState, CanProtocol.

TODO: a dedicated `docs/architecture/verification-strategy.md` is not
yet written.

## Ground Station Software

The ground station consists of three decoupled components:

1. **Packet Receiver** -- receives and decodes packets, stores raw data
2. **Backend** -- serves the frontend and distributes data to clients
3. **Frontend** -- displays received data to operators

The decoupling improves error tolerance: if the backend fails, the packet
receiver continues storing data. Raw incoming traffic is captured via
Wireshark to a `.pcap` file as an additional safety net.

See [`ground-station/README.md`](ground-station/README.md) and the
[ground-station guide](docs/guides/running-ground-station.md).

## Related Documents

- **[System Invariants](docs/standards/system-invariants.md)** -- I-1 through I-7
- **[Architecture Decision Records](docs/decisions/)** -- All ADRs
- **[Interface Control Documents](docs/interfaces/)** -- All ICDs
- **[Standards](docs/standards/)** -- Coding and process standards
- **[Source Code](flight-software/)** -- Implementation

## Document Maintenance

This document gives a high-level architecture overview for the REXUS 37
mission. Detailed descriptions live in linked documents. When architecture
changes, this document and the linked documents are updated together.
