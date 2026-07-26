#pragma once

#include "board/rs422_downlink.hpp"
#include "can_reassembler.hpp"
#include "self_test_sequencer.hpp"
#include <bolt/wire/header.hpp>
#include <bolt/wire/payloads.hpp>
#include <bolt/wire/types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace ExpForwarder {

    /// Read "EXP n finished its self-test" off a frame we forward anyway, or
    /// nullopt. NOT CRC-checked (the reassembler hands over raw bytes, ground
    /// validates) - the structural checks stand in. Bounded risk: TEST mode on
    /// the pad only, and the ground sees the same frame with its bad CRC
    ///
    /// @ingroup apps
    [[nodiscard]] inline std::optional<PacketProtocol::NodeId> test_done_node(const uint8_t* buf,
                                                                              uint8_t len) noexcept {
        using namespace PacketProtocol;

        constexpr uint8_t test_packet_len = static_cast<uint8_t>(HEADER_SIZE) +
                                            static_cast<uint8_t>(sizeof(PayloadTest)) + static_cast<uint8_t>(CRC_SIZE);
        constexpr uint8_t last_offset = static_cast<uint8_t>(HEADER_SIZE) + offsetof(PayloadTest, last);

        if (len != test_packet_len) {
            return std::nullopt;
        }
        if (buf[0] != SYNC_0 || buf[1] != SYNC_1 || buf[2] != PROTOCOL_VERSION) {
            return std::nullopt;
        }
        if (buf[HEADER_LENGTH_OFFSET] != sizeof(PayloadTest)) {
            return std::nullopt;
        }
        if (buf[last_offset] != 1U) {
            return std::nullopt; // a *_TEST step, but not the node's last
        }

        switch (static_cast<PayloadType>(buf[HEADER_TYPE_OFFSET])) {
        case PayloadType::EXP1_TEST:
            return NodeId::EXP1;
        case PayloadType::EXP2_TEST:
            return NodeId::EXP2;
        case PayloadType::EXP3_TEST:
            return NodeId::EXP3;
        default:
            return std::nullopt;
        }
    }

    /// Forward every fully-reassembled EXP CAN frame onto the RS-422 downlink,
    /// reading self-test progress off the frames as they pass
    ///
    /// @ingroup apps
    inline void drain(CanReassembler& reassembler, Rs422Downlink& downlink, SelfTestSequencer& sequencer) noexcept {
        std::array<uint8_t, PacketProtocol::MAX_PACKET_SIZE> buf{};
        uint8_t len = 0U;
        while (reassembler.pop(buf.data(), len)) {
            // forward first, inspect second - the peek must never cost a frame
            downlink.send(buf.data(), len);

            if (const auto done = test_done_node(buf.data(), len)) {
                sequencer.on_node_done(*done);
            }
        }
    }

} // namespace ExpForwarder
