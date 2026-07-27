#include "self_test_sequencer.hpp"

#include <catch2/catch_test_macros.hpp>

using PacketProtocol::NodeId;
using Stage = SelfTestSequencer::Stage;

TEST_CASE("full run advances BTC -> EXP1 -> EXP2 -> EXP3 -> IDLE", "[selftest]") {
    SelfTestSequencer seq;
    seq.start();
    REQUIRE(seq.current() == Stage::BTC);
    seq.on_node_done(NodeId::BTC);
    REQUIRE(seq.current() == Stage::EXP1);
    seq.on_node_done(NodeId::EXP1);
    REQUIRE(seq.current() == Stage::EXP2);
    seq.on_node_done(NodeId::EXP2);
    REQUIRE(seq.current() == Stage::EXP3);
    seq.on_node_done(NodeId::EXP3);
    REQUIRE(seq.current() == Stage::IDLE);
    REQUIRE(!seq.active());
}

TEST_CASE("a dark EXP is skipped when its silence budget runs dry", "[selftest]") {
    SelfTestSequencer seq;
    seq.start();
    seq.on_node_done(NodeId::BTC); // EXP1's turn - and it never answers

    for (uint16_t i = 0U; i < SelfTestSequencer::NODE_SILENCE_TICKS - 1U; ++i) {
        REQUIRE(!seq.on_tick().has_value());
    }
    REQUIRE(seq.current() == Stage::EXP1); // one tick short: still its turn

    const auto skipped = seq.on_tick();
    REQUIRE(skipped.has_value()); // budget dry: skipped, and the skip is named
    REQUIRE(*skipped == NodeId::EXP1);
    REQUIRE(seq.current() == Stage::EXP2); // the run continues
}

TEST_CASE("a step report from the node under test re-arms its budget", "[selftest]") {
    SelfTestSequencer seq;
    seq.start();
    seq.on_node_done(NodeId::BTC);

    // twice the budget in total, but never a full budget of silence
    for (uint16_t i = 0U; i < SelfTestSequencer::NODE_SILENCE_TICKS - 1U; ++i) {
        seq.on_tick();
    }
    seq.on_node_report(NodeId::EXP1); // a *_TEST step frame, not the last
    for (uint16_t i = 0U; i < SelfTestSequencer::NODE_SILENCE_TICKS - 1U; ++i) {
        seq.on_tick();
    }
    REQUIRE(seq.current() == Stage::EXP1); // alive nodes are never skipped

    seq.on_node_done(NodeId::EXP1);
    REQUIRE(seq.current() == Stage::EXP2);
}

TEST_CASE("done or report from any other node changes nothing", "[selftest]") {
    SelfTestSequencer seq;
    seq.start();
    seq.on_node_done(NodeId::BTC);
    REQUIRE(seq.current() == Stage::EXP1);

    seq.on_node_done(NodeId::BTC); // late duplicate must not skip EXP1
    seq.on_node_done(NodeId::EXP2);
    seq.on_node_report(NodeId::EXP3);
    REQUIRE(seq.current() == Stage::EXP1);
}

TEST_CASE("ticks outside an EXP stage never advance anything", "[selftest]") {
    SelfTestSequencer seq;
    for (uint32_t i = 0U; i < 1000U; ++i) {
        seq.on_tick();
    }
    REQUIRE(!seq.active()); // IDLE stays IDLE

    seq.start();
    for (uint32_t i = 0U; i < 1000U; ++i) {
        seq.on_tick();
    }
    REQUIRE(seq.current() == Stage::BTC); // the BTC's own turn has no budget
}
