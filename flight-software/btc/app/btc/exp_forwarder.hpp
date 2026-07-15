#pragma once

#include "board/rs422_downlink.hpp"
#include "can_reassembler.hpp"
#include <bolt/wire/header.hpp>

#include <array>
#include <cstdint>

namespace exp_forwarder {

/// Forward every fully-reassembled EXP CAN frame straight onto the RS-422
/// downlink
///
/// @ingroup apps
inline void drain(CanReassembler& reassembler, Rs422Downlink& downlink) noexcept {
    std::array<uint8_t, PacketProtocol::MAX_PACKET_SIZE> buf{};
    uint8_t len = 0U;
    while (reassembler.pop(buf.data(), len)) {
        downlink.send(buf.data(), len);
    }
}

} // namespace exp_forwarder
