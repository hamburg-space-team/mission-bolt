#include "utils/crc16.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>

// Standard CRC-16/CCITT-FALSE test vector: "123456789" -> 0x29B1
static constexpr std::array<uint8_t, 9> K_TEST_VEC = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

TEST_CASE("Crc::compute - known test vector", "[crc16]") {
    CHECK(Crc::compute(K_TEST_VEC.data(), K_TEST_VEC.size()) == 0x29B1);
}

TEST_CASE("Crc::compute - empty input returns INIT", "[crc16]") {
    CHECK(Crc::compute(nullptr, 0) == Crc::INIT);
}

TEST_CASE("Crc::compute - single zero byte", "[crc16]") {
    constexpr std::array<uint8_t, 1> BUF = {0x00};
    constexpr uint16_t EXPECTED = Crc::compute(BUF.data(), BUF.size());
    CHECK(Crc::compute(BUF.data(), BUF.size()) == EXPECTED);
}

TEST_CASE("Crc::update matches Crc::compute", "[crc16]") {
    uint16_t incremental = Crc::INIT;
    for (uint8_t byte : K_TEST_VEC) {
        incremental = Crc::update(incremental, byte);
    }
    CHECK(incremental == Crc::compute(K_TEST_VEC.data(), K_TEST_VEC.size()));
}

TEST_CASE("Crc::compute - different data gives different CRC", "[crc16]") {
    constexpr std::array<uint8_t, 2> DATA_A = {0xAA, 0xBB};
    constexpr std::array<uint8_t, 2> DATA_B = {0xAA, 0xBC};
    CHECK(Crc::compute(DATA_A.data(), DATA_A.size()) != Crc::compute(DATA_B.data(), DATA_B.size()));
}

TEST_CASE("Crc::append writes big-endian CRC and returns len+2", "[crc16]") {
    std::array<uint8_t, 16> buf = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    std::size_t total = Crc::append(buf.data(), 9);

    CHECK(total == 11);

    uint16_t appended = static_cast<uint16_t>(buf[9]) << 8 | buf[10];
    CHECK(appended == 0x29B1);
}

TEST_CASE("Crc::verify - valid frame passes", "[crc16]") {
    std::array<uint8_t, 16> buf = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    std::size_t total = Crc::append(buf.data(), 9);
    CHECK(Crc::verify(buf.data(), total));
}

TEST_CASE("Crc::verify - corrupted byte fails", "[crc16]") {
    std::array<uint8_t, 16> buf = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    std::size_t total = Crc::append(buf.data(), 9);
    buf[4] ^= 0xFF;
    CHECK_FALSE(Crc::verify(buf.data(), total));
}

TEST_CASE("Crc::verify - corrupted CRC byte fails", "[crc16]") {
    std::array<uint8_t, 16> buf = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    std::size_t total = Crc::append(buf.data(), 9);
    buf[total - 1] ^= 0x01;
    CHECK_FALSE(Crc::verify(buf.data(), total));
}

TEST_CASE("Crc::verify - size < 3 always false", "[crc16]") {
    std::array<uint8_t, 2> buf = {0xDE, 0xAD};
    CHECK_FALSE(Crc::verify(buf.data(), 0));
    CHECK_FALSE(Crc::verify(buf.data(), 1));
    CHECK_FALSE(Crc::verify(buf.data(), 2));
}

TEST_CASE("Crc::compute is constexpr", "[crc16]") {
    static_assert(Crc::compute(K_TEST_VEC.data(), K_TEST_VEC.size()) == 0x29B1);
}
