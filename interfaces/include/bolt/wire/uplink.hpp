/// THHOR-BOLT REXUS 37 - uplink (telecommand) contract
///
/// @ingroup comms

#pragma once

#include <bolt/wire/annotations.hpp>

#include <cstdint>

namespace PacketProtocol {

    /// Uplink command opcodes (RXSM TC -> BTC)
    /// @ingroup comms
    enum class CommandOpcode : uint8_t {
        RESET_TICK WIRE(.desc = "reset the 25 Hz tick counter to 0", .label = "Reset Tick", .danger = true) = 0x01,
        START_EXPERIMENT WIRE(.desc = "arm the experiment sequence", .label = "Start Experiment") = 0x02,
        ACTIVATE_CAMERA WIRE(.desc = "power the onboard camera", .label = "Activate Camera") = 0x03,
        FULL_SYSTEM_TEST WIRE(.desc = "run the built-in self test", .label = "Full System Test", .danger = true) = 0x04,
    };

    /// Result reported back in PayloadCmdAck.status.
    /// @ingroup comms
    enum class CommandAckStatus : uint8_t {
        ACCEPTED WIRE(.desc = "known command received (handler is a no-op for now)") = 0x00,
        UNKNOWN_OPCODE WIRE(.desc = "opcode not recognised - nothing done") = 0x01,
    };

} // namespace PacketProtocol
