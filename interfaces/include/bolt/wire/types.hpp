#pragma once
#include <bolt/wire/annotations.hpp>

#include <cstdint>

namespace PacketProtocol {

    /// Wire value of the type byte in PacketHeader
    ///
    /// @ingroup comms
    enum class PayloadType : uint8_t {
        // BTC
        BTC_ENV = 0x10,
        BTC_STATUS = 0x11,
        BTC_IMU = 0x12,

        // EXP1 - Space Disco
        EXP1_SPECTRUM = 0x20,
        EXP1_ENV = 0x22,
        EXP1_STATUS = 0x23,

        // EXP2 - Bouncy Castle
        EXP2_BER = 0x30,
        EXP2_STATUS = 0x31,
        EXP2_ENV = 0x32,

        // EXP3 - Floaty Boi
        EXP3_STACK_A = 0x40,
        EXP3_STACK_B = 0x41,
        EXP3_ENV = 0x42,
        EXP3_STATUS = 0x43,
        EXP3_IMU = 0x44,

        // System
        GAP_MARKER = 0xF0,
        FAULT = 0xF1,
        CMD_ACK = 0xF2,
        TIMING = 0xF3,
        BOOT = 0xFE,
    };

    /// @ingroup comms
    enum class BootReason : uint8_t {
        COLD_START WIRE(.desc = "normal power-on") = 0x01,
        WATCHDOG WIRE(.desc = "IWDG fired, main loop had hung") = 0x02,
        SOFT_RESET WIRE(.desc = "intentional software reset") = 0x03,
    };

    /// Per CDR section 4.3.5 / ADR-005.
    /// @ingroup comms
    enum class GapReason : uint8_t {
        NO_DATA WIRE(.desc = "no valid CAN/source data inside the expected window") = 0x01,
        CAN_CRC_FAIL WIRE(.desc = "CAN frame received but failed CRC") = 0x02,
        LIFI_TIMEOUT WIRE(.desc = "no sample packets from an EXP3 stack in the interval") = 0x03,
        SENSOR_FAILED WIRE(.desc = "superseded by FAULT (ADR-012); wire-stable, no longer emitted") = 0x04,
    };

    /// Origin node of a FAULT / GAP_MARKER / BOOT packet
    /// @ingroup comms
    enum class NodeId : uint8_t {
        BTC WIRE(.desc = "master controller") = 0x00,
        EXP1 WIRE(.desc = "Space Disco spectrometer") = 0x01,
        EXP2 WIRE(.desc = "Bouncy Castle Li-Fi") = 0x02,
        EXP3 WIRE(.desc = "Floaty Boi stacks") = 0x03,
        UNKNOWN WIRE(.desc = "unknown origin") = 0xFF,
    };

} // namespace PacketProtocol
