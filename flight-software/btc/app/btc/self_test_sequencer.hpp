#pragma once

#include "can_protocol.hpp"
#include <bolt/wire/types.hpp>

#include <cstdint>

/// Orders one full-system self-test: BTC first, then EXP1/EXP2/EXP3. Only
/// decides WHOSE turn it is - each node's Runner picks its own steps and
/// signals done via its *_TEST packet with last=1. The turn is a level in
/// SYNC byte [3] (a dropped frame costs one tick, not the run); the BTC
/// reads "done" off the frames it already forwards to ground. TEST mode
/// only, LO aborts
///
/// @ingroup apps
class SelfTestSequencer {
  public:
    enum class Stage : uint8_t { IDLE, BTC, EXP1, EXP2, EXP3 };

    /// Begin at the BTC (restarts a run in progress)
    void start() noexcept {
        stage = Stage::BTC;
    }

    void abort() noexcept {
        stage = Stage::IDLE;
    }

    [[nodiscard]] bool active() const noexcept {
        return stage != Stage::IDLE;
    }

    [[nodiscard]] Stage current() const noexcept {
        return stage;
    }

    [[nodiscard]] bool btcs_turn() const noexcept {
        return stage == Stage::BTC;
    }

    /// SYNC byte [3]: the node whose turn it is, or SELF_TEST_NONE
    [[nodiscard]] uint8_t sync_target() const noexcept {
        switch (stage) {
        case Stage::EXP1:
            return static_cast<uint8_t>(PacketProtocol::NodeId::EXP1);
        case Stage::EXP2:
            return static_cast<uint8_t>(PacketProtocol::NodeId::EXP2);
        case Stage::EXP3:
            return static_cast<uint8_t>(PacketProtocol::NodeId::EXP3);
        case Stage::IDLE:
        case Stage::BTC:
        default:
            // the BTC's own turn is not broadcast - it runs locally
            return CanProtocol::SELF_TEST_NONE;
        }
    }

    /// Advance past the node that reported its last step; a report from any
    /// other node is ignored (a late duplicate must not skip the next one)
    void on_node_done(PacketProtocol::NodeId node) noexcept {
        switch (stage) {
        case Stage::BTC:
            if (node == PacketProtocol::NodeId::BTC) {
                stage = Stage::EXP1;
            }
            break;
        case Stage::EXP1:
            if (node == PacketProtocol::NodeId::EXP1) {
                stage = Stage::EXP2;
            }
            break;
        case Stage::EXP2:
            if (node == PacketProtocol::NodeId::EXP2) {
                stage = Stage::EXP3;
            }
            break;
        case Stage::EXP3:
            if (node == PacketProtocol::NodeId::EXP3) {
                stage = Stage::IDLE;
            }
            break;
        case Stage::IDLE:
        default:
            break;
        }
    }

  private:
    Stage stage = Stage::IDLE;
};
