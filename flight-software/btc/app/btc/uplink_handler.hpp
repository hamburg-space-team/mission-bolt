#pragma once

#include <bolt/wire/uplink.hpp>

#include <cstdint>

namespace PacketProtocol::UplinkHandler {

    /// Classify an inbound uplink command opcode into the ACK status the BTC returns
    ///
    /// @ingroup apps
    [[nodiscard]] inline CommandAckStatus classify(uint8_t opcode) noexcept {
        switch (static_cast<CommandOpcode>(opcode)) {
        case CommandOpcode::RESET_TICK:
        case CommandOpcode::START_EXPERIMENT:
        case CommandOpcode::ACTIVATE_CAMERA:
        case CommandOpcode::STOP_EXPERIMENT:
        case CommandOpcode::FULL_SYSTEM_TEST:
            return CommandAckStatus::ACCEPTED;
        default:
            return CommandAckStatus::UNKNOWN_OPCODE;
        }
    }

} // namespace PacketProtocol::UplinkHandler
