#pragma once
#include <cstdint>

namespace PacketProtocol {

    /// Wire value of the type byte in PacketHeader. High nibble = source
    /// controller, so ground can route without parsing the payload.
    ///
    /// @ingroup comms
    enum class PayloadType : uint8_t {
        // BTC
        BTC_ENV = 0x10,    ///< PayloadBtcEnv
        BTC_STATUS = 0x11, ///< PayloadBtcStatus
        BTC_IMU = 0x12,    ///< PayloadBtcImu - one sample per IMU data-ready

        // EXP1 - Space Disco
        EXP1_SPECTRUM_A = 0x20, ///< PayloadExp1SpectrumA - AS7265X channels 1-9
        EXP1_SPECTRUM_B = 0x21, ///< PayloadExp1SpectrumB - AS7265X channels 10-18
        EXP1_ENV = 0x22,        ///< PayloadExpEnv (shared with EXP2)
        EXP1_STATUS = 0x23,     ///< PayloadExpStatus (shared with EXP2)

        // EXP2 - Bouncy Castle
        EXP2_BER = 0x30,    ///< PayloadExp2Ber
        EXP2_STATUS = 0x31, ///< PayloadExpStatus (shared with EXP1)
        EXP2_ENV = 0x32,    ///< PayloadExpEnv (shared with EXP1)

        // EXP3 - Floaty Boi
        EXP3_STACK_A = 0x40, ///< PayloadExp3StackA - wired stack sample
        EXP3_STACK_B = 0x41, ///< PayloadExp3StackB - wireless stack sample
        EXP3_ENV = 0x42,     ///< PayloadExpEnv (shared: TMP117 + MS5611, no IMU)
        EXP3_STATUS = 0x43,  ///< PayloadExp3Status
        EXP3_IMU = 0x44,     ///< PayloadExp3Imu - controller ICM-42686 sample

        // System
        GAP_MARKER = 0xF0, ///< PayloadGapMarker
        FAULT = 0xF1,      ///< PayloadFault - latched fault + error step trace (ADR-012)
        CMD_ACK = 0xF2,    ///< PayloadCmdAck - acknowledges a received uplink command
        BOOT = 0xFE,       ///< PayloadBoot
    };

    /// Uplink command opcodes (RXSM TC -> BTC)
    /// @ingroup comms
    enum class CommandOpcode : uint8_t {
        RESET_TICK = 0x01,
        START_EXPERIMENT = 0x02,
        ACTIVATE_CAMERA = 0x03,
        FULL_SYSTEM_TEST = 0x04,
    };

    /// Result reported back in PayloadCmdAck.status
    /// @ingroup comms
    enum class CommandAckStatus : uint8_t {
        ACCEPTED = 0x00,       ///< Known command received (handler is a no-op for now)
        UNKNOWN_OPCODE = 0x01, ///< Opcode not recognised - nothing done
    };

    /// @ingroup comms
    enum class BootReason : uint8_t {
        COLD_START = 0x01, ///< Normal power-on
        WATCHDOG = 0x02,   ///< IWDG fired, main loop had hung
        SOFT_RESET = 0x03, ///< Intentional software reset
    };

    /// Per CDR section 4.3.5 / ADR-005.
    /// @ingroup comms
    enum class GapReason : uint8_t {
        NO_DATA = 0x01,       ///< No valid CAN/source data inside the expected window
        CAN_CRC_FAIL = 0x02,  ///< CAN frame received but failed CRC
        LIFI_TIMEOUT = 0x03,  ///< No sample packets from an EXP3 stack inside the expected interval
        SENSOR_FAILED = 0x04, ///< Superseded by PayloadType::FAULT (ADR-012) - value kept
                              ///< wire-stable, no longer emitted
    };

    /// Origin node of a FAULT / GAP_MARKER / BOOT packet
    /// @ingroup comms
    enum class NodeId : uint8_t {
        BTC = 0x00,
        EXP1 = 0x01,
        EXP2 = 0x02,
        EXP3 = 0x03,
        UNKNOWN = 0xFF,
    };

} // namespace PacketProtocol
