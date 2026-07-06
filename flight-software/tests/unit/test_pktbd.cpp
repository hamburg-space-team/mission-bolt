#include "packet_builder.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace PacketProtocol;

TEST_CASE("PacketBuilder-payload to large", "[packet_builder]"){
    PacketBuilder builder;
    builder.init();

    std::array<uint8_t, MAX_PACKET_SIZE> buf{};
    std::array<uint8_t, MAX_PACKET_SIZE + 1> payload{};

    CHECK(builder.build(buf.data(),
                        PayloadType::BOOT,
                        Tick{0},
                        TimestampUs{0},
                        payload.data(),
                        payload.size()) == 0);

}