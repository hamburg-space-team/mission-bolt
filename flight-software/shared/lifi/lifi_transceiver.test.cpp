#include "lifi/lifi_transceiver.hpp"

#include "lifi/lifi_pattern.hpp"
#include "lifi/lifi_protocol.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>

// ---------------------------------------------------------------------------
// Fake link: what the tick loop writes is captured, what the "transceiver"
// answers is fed in with a receive stamp. Clocks are plain globals the test
// advances by hand.
// ---------------------------------------------------------------------------
namespace {

    uint32_t clock_ms_g = 1000U;
    uint32_t clock_us_g = 1000000U;

    uint32_t fake_ms() {
        return clock_ms_g;
    }
    uint32_t fake_us() {
        return clock_us_g;
    }
    void advance(uint32_t ms) {
        clock_ms_g += ms;
        clock_us_g += ms * 1000U;
    }

    class FakeUartLink final : public UartLink {
      public:
        bool write(std::span<const uint8_t> data) noexcept override {
            if (this->refuse) {
                return false;
            }
            this->tx.insert(this->tx.end(), data.begin(), data.end());
            return true;
        }
        std::size_t read(std::span<uint8_t> out) noexcept override {
            const std::size_t n = out.size() < this->rx.size() ? out.size() : this->rx.size();
            for (std::size_t i = 0U; i < n; i++) {
                out[i] = this->rx[i];
            }
            this->rx.erase(this->rx.begin(), this->rx.begin() + static_cast<std::ptrdiff_t>(n));
            return n;
        }
        uint32_t last_rx_ms() const noexcept override {
            return this->rx_ms;
        }
        uint32_t last_rx_us() const noexcept override {
            return this->rx_us;
        }
        uint16_t rx_dropped() const noexcept override {
            return 0U;
        }

        /// The transceiver answers `bytes` now
        void answer(std::initializer_list<uint8_t> bytes) {
            answer(std::span<const uint8_t>(bytes.begin(), bytes.size()));
        }
        void answer(std::span<const uint8_t> bytes) {
            this->rx.insert(this->rx.end(), bytes.begin(), bytes.end());
            this->rx_ms = clock_ms_g;
            this->rx_us = clock_us_g;
        }
        void acks(uint8_t n) {
            for (uint8_t i = 0U; i < n; i++) {
                answer({LifiProtocol::ACK});
            }
        }

        std::vector<uint8_t> tx;
        std::vector<uint8_t> rx;
        bool refuse = false;
        uint32_t rx_ms = 0U;
        uint32_t rx_us = 0U;
    };

    constexpr auto PATTERN = LifiPattern::make<64U>(LifiPattern::Kind::PRBS7);

    bool tx_equals(const FakeUartLink& link, std::initializer_list<uint8_t> expected) {
        return std::vector<uint8_t>(expected) == link.tx;
    }

} // namespace

// --- protocol encoders -------------------------------------------------------

TEST_CASE("Script encodes commands big-endian as ICD-008 shows them", "[lifi]") {
    using LifiProtocol::Cmd;
    LifiProtocol::Script<16U> s;
    s.u16(Cmd::SET_BUFFER, 2U).u32(Cmd::SET_OPTICAL_BAUD, 460800U).u16(Cmd::SET_THRESHOLD, 2500U);
    const std::vector<uint8_t> expected = {0x5CU, 0x4EU, 0x00U, 0x02U, 0x5CU, 0x66U, 0x00U,
                                           0x07U, 0x08U, 0x00U, 0x5CU, 0x54U, 0x09U, 0xC4U};
    const auto got = s.bytes();
    CHECK(std::vector<uint8_t>(got.begin(), got.end()) == expected);
    CHECK_FALSE(s.overflowed());
}

TEST_CASE("Script overflow is sticky and leaves the capacity intact", "[lifi]") {
    LifiProtocol::Script<3U> s;
    s.u16(LifiProtocol::Cmd::SET_BUFFER, 64U);
    CHECK(s.overflowed());
    CHECK(s.bytes().size() == 3U);
    s.clear();
    CHECK_FALSE(s.overflowed());
    CHECK(s.bytes().empty());
}

TEST_CASE("frame_time_ms rounds up", "[lifi]") {
    CHECK(LifiProtocol::frame_time_ms(2000U, 115200U) == 174U); // ICD-008: 174 ms
    CHECK(LifiProtocol::frame_time_ms(2000U, 460800U) == 44U);  // 43.4 ms -> 44
    CHECK(LifiProtocol::frame_time_ms(64U, 460800U) == 2U);     // 1.39 ms -> 2
    CHECK(LifiProtocol::frame_time_ms(68U, 115200U) == 6U);     // 5.9 ms -> 6
}

// --- patterns ----------------------------------------------------------------

TEST_CASE("PRBS-7 has period 127 and is balanced", "[lifi][pattern]") {
    const auto block = LifiPattern::make<32U>(LifiPattern::Kind::PRBS7); // 256 bits, two periods
    auto bit = [&](std::size_t i) { return (block[i / 8U] >> (7U - (i % 8U))) & 1U; };
    unsigned ones = 0U;
    for (std::size_t i = 0U; i < 127U; i++) {
        CHECK(bit(i) == bit(i + 127U));
        ones += bit(i);
    }
    CHECK(ones == 64U);
    // not a degenerate sequence
    CHECK(block[0] != block[1]);
}

TEST_CASE("PRBS-15 differs from PRBS-7, fixed patterns are what ICD-003 says", "[lifi][pattern]") {
    const auto p7 = LifiPattern::make<8U>(LifiPattern::Kind::PRBS7);
    const auto p15 = LifiPattern::make<8U>(LifiPattern::Kind::PRBS15);
    CHECK(p7 != p15);
    for (const uint8_t b : LifiPattern::make<4U>(LifiPattern::Kind::ALL_ONES)) {
        CHECK(b == 0xFFU);
    }
    for (const uint8_t b : LifiPattern::make<4U>(LifiPattern::Kind::ALTERNATING)) {
        CHECK(b == 0xAAU);
    }
}

TEST_CASE("Seeded PRBS-15 is reproducible and the seed selects the block", "[lifi][pattern]") {
    std::array<uint8_t, 64U> a{};
    std::array<uint8_t, 64U> b{};
    LifiPattern::fill_prbs15(a, 0x1234U);
    LifiPattern::fill_prbs15(b, 0x1234U);
    CHECK(a == b); // the ground regenerates exactly this from the seed

    LifiPattern::fill_prbs15(b, 0x1235U);
    CHECK(a != b);

    // the unseeded kind is seed 0x7FFF, and seed 0 must not lock the LFSR
    LifiPattern::fill_prbs15(a, 0x7FFFU);
    CHECK(a == LifiPattern::make<64U>(LifiPattern::Kind::PRBS15));
    LifiPattern::fill_prbs15(b, 0x0000U);
    CHECK(b == a);
    // bit 15 is not part of the 15-bit state
    LifiPattern::fill_prbs15(b, 0x8000U | 0x1234U);
    LifiPattern::fill_prbs15(a, 0x1234U);
    CHECK(a == b);
}

// --- sender ------------------------------------------------------------------

TEST_CASE("Sender configure writes the ICD-008 sender script and waits for 4 ACKs", "[lifi][sender]") {
    FakeUartLink link;
    LifiSender tx{link, fake_ms, fake_us};
    link.answer({0x48U, 0x65U, 0x6CU}); // boot banner fragment, must be flushed

    REQUIRE(tx.configure(64U, 115200U).has_value());
    CHECK(tx_equals(link, {0x5CU, 0x50U, 0x5CU, 0x58U, 0x5CU, 0x66U, 0x00U, 0x01U, 0xC2U, 0x00U, 0x5CU, 0x4EU, 0x00U,
                           0x40U}));
    CHECK(tx.pending());
    CHECK(link.rx.empty());

    link.acks(3U);
    REQUIRE(tx.poll().has_value());
    CHECK(tx.pending());
    link.acks(1U);
    REQUIRE(tx.poll().has_value());
    CHECK_FALSE(tx.pending());
    CHECK(tx.ready());
    CHECK(tx.block_len() == 64U);
    CHECK(tx.optical_baud() == 115200U);
}

TEST_CASE("Sender rejects what the transceiver would NAK or mishandle", "[lifi][sender]") {
    FakeUartLink link;
    LifiSender tx{link, fake_ms, fake_us};

    auto r = tx.configure(0U, 115200U);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == ErrorCode::BAD_ARGUMENT);
    CHECK(r.error().trace[0] == Step::LIFI_CONFIGURE);
    CHECK_FALSE(tx.configure(256U, 115200U).has_value()); // limitation #1
    CHECK_FALSE(tx.configure(64U, 921600U).has_value());  // optics stop at 460 800
    CHECK_FALSE(tx.configure(64U, 200U).has_value());
    CHECK(link.tx.empty());

    REQUIRE(tx.configure(64U, 115200U).has_value());
    link.tx.clear();
    // a second setup while one is outstanding restarts it: the old chain
    // is dropped without a strike and the new script goes out in full
    REQUIRE(tx.configure(64U, 460800U).has_value());
    CHECK(tx.pending());
    CHECK(link.tx.size() == 14U);
    CHECK_FALSE(tx.is_failed());
    link.acks(4U);
    REQUIRE(tx.poll().has_value());
    CHECK_FALSE(tx.pending());
    CHECK(tx.optical_baud() == 460800U);
}

TEST_CASE("load_and_send while a chain is outstanding is a caller error, not a strike", "[lifi][sender]") {
    FakeUartLink link;
    LifiSender tx{link, fake_ms, fake_us};
    REQUIRE(tx.configure(64U, 115200U).has_value());
    link.acks(4U);
    REQUIRE(tx.poll().has_value());
    advance(1U);
    REQUIRE(tx.load_and_send(PATTERN).has_value());

    auto again = tx.load_and_send(PATTERN);
    REQUIRE_FALSE(again.has_value());
    CHECK(again.error() == ErrorCode::BAD_ARGUMENT);
    CHECK(again.error().trace[0] == Step::LIFI_LOAD_SEND);
    CHECK_FALSE(tx.is_failed());
    CHECK(tx.pending());
}

TEST_CASE("Sender load_and_send streams exactly the block and tracks the optical time", "[lifi][sender]") {
    FakeUartLink link;
    LifiSender tx{link, fake_ms, fake_us};
    REQUIRE(tx.configure(64U, 460800U).has_value());
    link.acks(4U);
    REQUIRE(tx.poll().has_value());
    link.tx.clear();

    // a short block would leave the transceiver waiting for payload and
    // eat the '\S' as data: refused before anything goes out
    CHECK_FALSE(tx.load_and_send(std::span<const uint8_t>(PATTERN.data(), 63U)).has_value());
    CHECK(link.tx.empty());

    clock_us_g = 5000000U;
    REQUIRE(tx.load_and_send(PATTERN).has_value());
    REQUIRE(link.tx.size() == 68U);
    CHECK(link.tx[0] == 0x5CU);
    CHECK(link.tx[1] == 0x73U);
    CHECK(link.tx[66] == 0x5CU);
    CHECK(link.tx[67] == 0x53U);
    CHECK(std::vector<uint8_t>(link.tx.begin() + 2, link.tx.begin() + 66) ==
          std::vector<uint8_t>(PATTERN.begin(), PATTERN.end()));
    CHECK(tx.last_send_us() == 5000000U);
    CHECK(tx.pending());
    CHECK_FALSE(tx.ready());

    link.acks(1U);
    REQUIRE(tx.poll().has_value());
    CHECK_FALSE(tx.pending());
    // upload 68 B at 115200 (6 ms) + 64 B at 460800 (2 ms): busy until then
    CHECK_FALSE(tx.optical_done());
    advance(7U);
    CHECK_FALSE(tx.optical_done());
    advance(1U);
    CHECK(tx.optical_done());
    CHECK(tx.ready());
}

TEST_CASE("NAK drops the chain with the operation in the trace and counts a strike", "[lifi][sender]") {
    FakeUartLink link;
    LifiSender tx{link, fake_ms, fake_us};
    REQUIRE(tx.configure(64U, 115200U).has_value());
    link.acks(2U);
    link.answer({LifiProtocol::NAK});
    link.acks(1U);

    auto r = tx.poll();
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == ErrorCode::PROTOCOL_ERROR);
    CHECK(r.error().depth == 2U);
    CHECK(r.error().trace[0] == Step::LIFI_REPLY);
    CHECK(r.error().trace[1] == Step::LIFI_CONFIGURE);
    CHECK_FALSE(tx.pending());
    CHECK_FALSE(tx.is_failed());
    // the byte after the NAK belongs to nobody: discarded, not a second fault
    CHECK(tx.poll().has_value());
}

TEST_CASE("Three failed chains latch the sender, a completed one clears the strikes", "[lifi][sender]") {
    FakeUartLink link;
    LifiSender tx{link, fake_ms, fake_us};

    for (int strike = 0; strike < 2; strike++) {
        REQUIRE(tx.configure(64U, 115200U).has_value());
        link.answer({LifiProtocol::NAK});
        REQUIRE_FALSE(tx.poll().has_value());
    }
    CHECK_FALSE(tx.is_failed());

    // a full chain of ACKs resets the count (ADR-005)
    REQUIRE(tx.configure(64U, 115200U).has_value());
    link.acks(4U);
    REQUIRE(tx.poll().has_value());

    for (int strike = 0; strike < 3; strike++) {
        REQUIRE(tx.configure(64U, 115200U).has_value());
        link.answer({LifiProtocol::NAK});
        REQUIRE_FALSE(tx.poll().has_value());
    }
    CHECK(tx.is_failed());
    CHECK(tx.last_error() == ErrorCode::PROTOCOL_ERROR);

    auto r = tx.configure(64U, 115200U);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == ErrorCode::DISABLED);

    tx.clear_latch();
    CHECK(tx.configure(64U, 115200U).has_value());
}

TEST_CASE("Silence past the deadline is a TIMEOUT, silence before it is not", "[lifi][sender]") {
    FakeUartLink link;
    LifiSender tx{link, fake_ms, fake_us};
    REQUIRE(tx.configure(64U, 115200U).has_value()); // 14 B script -> 2 ms on the wire + 20 ms

    advance(21U);
    REQUIRE(tx.poll().has_value());
    CHECK(tx.pending());

    advance(1U);
    auto r = tx.poll();
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == ErrorCode::TIMEOUT);
    CHECK(r.error().trace[0] == Step::LIFI_REPLY);
    CHECK(r.error().trace[1] == Step::LIFI_CONFIGURE);
    CHECK_FALSE(tx.pending());
}

TEST_CASE("A stray byte where the sender expects an ACK is a bus fault", "[lifi][sender]") {
    FakeUartLink link;
    LifiSender tx{link, fake_ms, fake_us};
    REQUIRE(tx.configure(64U, 115200U).has_value());
    link.answer({0x48U});
    auto r = tx.poll();
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == ErrorCode::BUS_ERROR);
    CHECK(r.error().trace[0] == Step::LIFI_REPLY);
}

TEST_CASE("A refused link write is a strike with the link in the trace", "[lifi][sender]") {
    FakeUartLink link;
    LifiSender tx{link, fake_ms, fake_us};
    link.refuse = true;
    auto r = tx.configure(64U, 115200U);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == ErrorCode::BUS_ERROR);
    CHECK(r.error().trace[0] == Step::LIFI_LINK_WRITE);
    CHECK(r.error().trace[1] == Step::LIFI_CONFIGURE);
    CHECK_FALSE(tx.pending());
}

TEST_CASE("Sender set_rate resizes the buffer with the new rate", "[lifi][sender]") {
    FakeUartLink link;
    LifiSender tx{link, fake_ms, fake_us};
    REQUIRE(tx.configure(64U, 115200U).has_value());
    link.acks(4U);
    REQUIRE(tx.poll().has_value());
    link.tx.clear();

    REQUIRE(tx.set_rate(9600U, 16U).has_value());
    CHECK(tx_equals(link, {0x5CU, 0x66U, 0x00U, 0x00U, 0x25U, 0x80U, 0x5CU, 0x4EU, 0x00U, 0x10U}));
    link.acks(2U);
    REQUIRE(tx.poll().has_value());
    CHECK(tx.block_len() == 16U);
    CHECK(tx.optical_baud() == 9600U);
    CHECK(tx.load_and_send(std::span<const uint8_t>(PATTERN.data(), 16U)).has_value());
}

TEST_CASE("ping writes a bare '\\P' and passes on one ACK", "[lifi]") {
    FakeUartLink link;
    LifiSender tx{link, fake_ms, fake_us};
    link.answer({0x0AU, 0x0DU});
    REQUIRE(tx.ping().has_value());
    CHECK(tx_equals(link, {0x5CU, 0x50U}));
    CHECK(link.rx.empty());
    link.acks(1U);
    REQUIRE(tx.poll().has_value());
    CHECK_FALSE(tx.pending());
}

// --- receiver ----------------------------------------------------------------

namespace {
    void bring_up(FakeUartLink& link, LifiReceiver& rx, uint16_t block_len = 64U) {
        REQUIRE(rx.configure(block_len, 115200U, 2500U, 5U).has_value());
        link.acks(7U);
        REQUIRE(rx.poll().has_value());
        REQUIRE_FALSE(rx.pending());
        link.tx.clear();
    }
} // namespace

TEST_CASE("Receiver configure writes the ICD-008 receiver script, '\\R' last", "[lifi][receiver]") {
    FakeUartLink link;
    LifiReceiver rx{link, fake_ms, fake_us};
    REQUIRE(rx.configure(64U, 460800U, 2500U, 5U).has_value());
    CHECK(tx_equals(link, {0x5CU, 0x50U,                                 // \P
                           0x5CU, 0x58U,                                 // \X
                           0x5CU, 0x4EU, 0x00U, 0x40U,                   // \N 64
                           0x5CU, 0x54U, 0x09U, 0xC4U,                   // \T 2500
                           0x5CU, 0x66U, 0x00U, 0x07U, 0x08U, 0x00U,     // \f 460800
                           0x5CU, 0x46U,                                 // \F
                           0x5CU, 0x52U}));                              // \R
    CHECK(rx.pending());
    CHECK_FALSE(rx.configure(64U, 115200U, 4096U, 5U).has_value()); // threshold range
}

TEST_CASE("Receiver collects a block and judges it complete", "[lifi][receiver]") {
    FakeUartLink link;
    LifiReceiver rx{link, fake_ms, fake_us};
    bring_up(link, rx);
    CHECK(rx.block_state() == LifiReceiver::Block::EMPTY);

    clock_us_g = 7000000U;
    link.answer(std::span<const uint8_t>(PATTERN.data(), 40U));
    REQUIRE(rx.poll().has_value());
    CHECK(rx.block_state() == LifiReceiver::Block::FILLING);
    advance(2U);
    link.answer(std::span<const uint8_t>(PATTERN.data() + 40U, 24U));
    REQUIRE(rx.poll().has_value());
    CHECK(rx.block_state() == LifiReceiver::Block::COMPLETE);
    CHECK(rx.last_byte_us() == 7002000U);

    const auto ber = rx.compare(PATTERN);
    CHECK(ber.bits_sent == 512U);
    CHECK(ber.bit_errors == 0U);
    CHECK(ber.missing_bytes == 0U);
    CHECK(ber.overflow_bytes == 0U);
    CHECK(ber.first_error_byte == LifiReceiver::NO_ERROR_BYTE);
    CHECK(ber.last_error_byte == LifiReceiver::NO_ERROR_BYTE);
    CHECK(ber.first_byte_ok);

    CHECK(rx.take() == 64U);
    CHECK(rx.block_state() == LifiReceiver::Block::EMPTY);
}

TEST_CASE("Receiver counts bit errors and locates the first and last bad byte", "[lifi][receiver]") {
    FakeUartLink link;
    LifiReceiver rx{link, fake_ms, fake_us};
    bring_up(link, rx);

    auto damaged = PATTERN;
    damaged[3] ^= 0x01U;  // 1 bit
    damaged[50] ^= 0xF0U; // 4 bits
    link.answer(damaged);
    REQUIRE(rx.poll().has_value());

    const auto ber = rx.compare(PATTERN);
    CHECK(ber.bit_errors == 5U);
    CHECK(ber.first_error_byte == 3U);
    CHECK(ber.last_error_byte == 50U);
    CHECK(ber.first_byte_ok);
    CHECK(ber.missing_bytes == 0U);
}

TEST_CASE("A partial block goes COOLDOWN after the quiet time and counts missing bytes", "[lifi][receiver]") {
    FakeUartLink link;
    LifiReceiver rx{link, fake_ms, fake_us};
    bring_up(link, rx);

    auto damaged = PATTERN;
    damaged[0] ^= 0xFFU; // frame desync: the very first byte is wrong
    link.answer(std::span<const uint8_t>(damaged.data(), 60U));
    REQUIRE(rx.poll().has_value());
    CHECK(rx.block_state() == LifiReceiver::Block::FILLING);
    advance(4U);
    CHECK(rx.block_state() == LifiReceiver::Block::FILLING);
    advance(1U);
    CHECK(rx.block_state() == LifiReceiver::Block::COOLDOWN);

    const auto ber = rx.compare(PATTERN);
    CHECK(ber.missing_bytes == 4U);
    CHECK(ber.bit_errors == 8U + (4U * 8U));
    CHECK(ber.first_error_byte == 0U);
    CHECK(ber.last_error_byte == 63U);
    CHECK_FALSE(ber.first_byte_ok);
}

TEST_CASE("Bytes beyond the block are counted as overflow and dropped", "[lifi][receiver]") {
    FakeUartLink link;
    LifiReceiver rx{link, fake_ms, fake_us};
    bring_up(link, rx, 16U);

    link.answer(std::span<const uint8_t>(PATTERN.data(), 20U));
    REQUIRE(rx.poll().has_value());
    CHECK(rx.block_state() == LifiReceiver::Block::COMPLETE);
    const auto ber = rx.compare(std::span<const uint8_t>(PATTERN.data(), 16U));
    CHECK(ber.bit_errors == 0U);
    CHECK(ber.overflow_bytes == 4U);
    CHECK(rx.block().size() == 16U);
}

TEST_CASE("Receiver set_rate tolerates data between its ACKs and starts a fresh block", "[lifi][receiver]") {
    FakeUartLink link;
    LifiReceiver rx{link, fake_ms, fake_us};
    bring_up(link, rx);
    link.answer(std::span<const uint8_t>(PATTERN.data(), 64U));
    REQUIRE(rx.poll().has_value());
    (void)rx.take();

    REQUIRE(rx.set_rate(38400U, 64U, 5U).has_value());
    CHECK(tx_equals(link, {0x5CU, 0x66U, 0x00U, 0x00U, 0x96U, 0x00U, 0x5CU, 0x52U}));
    // a straggling optical byte between the two ACKs: payload, not a fault
    link.answer({LifiProtocol::ACK, 0x42U, LifiProtocol::ACK});
    REQUIRE(rx.poll().has_value());
    CHECK_FALSE(rx.pending());
    CHECK_FALSE(rx.is_failed());
    CHECK(rx.block().size() == 1U);
    CHECK(rx.block()[0] == 0x42U);
}

TEST_CASE("Receiver reset drops the outstanding chain and the collected bytes", "[lifi][receiver]") {
    FakeUartLink link;
    LifiReceiver rx{link, fake_ms, fake_us};
    bring_up(link, rx);
    link.answer(std::span<const uint8_t>(PATTERN.data(), 10U));
    REQUIRE(rx.poll().has_value());
    REQUIRE(rx.set_rate(38400U, 64U, 5U).has_value());
    link.answer({0x11U, 0x22U});

    rx.reset();
    CHECK_FALSE(rx.pending());
    CHECK(link.rx.empty());
    // the block itself survives reset(): take() is the caller's decision
    CHECK(rx.block().size() == 10U);
    (void)rx.take();
    CHECK(rx.block_state() == LifiReceiver::Block::EMPTY);
}
