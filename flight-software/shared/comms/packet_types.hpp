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
        EXP3_ENV = 0x42,     ///< PayloadExp3Env
        EXP3_STATUS = 0x43,  ///< PayloadExp3Status

        // System
        GAP_MARKER = 0xF0, ///< PayloadGapMarker
        BOOT = 0xFE,       ///< PayloadBoot
    };

    /// @ingroup comms
    enum class BootReason : uint8_t {
        COLD_START = 0x01, ///< Normal power-on
        WATCHDOG = 0x02,   ///< IWDG fired, main loop had hung
        SOFT_RESET = 0x03, ///< Intentional software reset
    };

    /// @ingroup comms
    enum class GapReason : uint8_t {
        SENSOR_FAILED = 0x01,     ///< sensor non-functional; EXP sends this over CAN to BTC
        SCHEDULER_OVERRUN = 0x02, ///< BTC main loop ran late; BTC sends this to RS-422 downlink
        SYNC_MISSED = 0x03,       ///< EXP did not receive expected BTC SYNC frame(s)
    };

} // namespace PacketProtocol
