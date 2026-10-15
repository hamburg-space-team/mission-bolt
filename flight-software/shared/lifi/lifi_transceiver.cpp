#include "lifi_transceiver.hpp"

#include <bit>

using LifiProtocol::Cmd;

// --- LifiTransceiver ---------------------------------------------------------

LifiTransceiver::LifiTransceiver(UartLink& link, tick_fn tick_ms, tick_fn tick_us) noexcept
    : link(link), tick_ms(tick_ms), tick_us(tick_us) {
}

void LifiTransceiver::on_data(std::span<const uint8_t> /*bytes*/) noexcept {
}

bool LifiTransceiver::stray_is_data() const noexcept {
    return false;
}

void LifiTransceiver::reset() noexcept {
    this->expected_acks = 0U;
    this->chain_step = Step::NONE;
    this->link.flush_rx();
}

Result<void> LifiTransceiver::submit(std::span<const uint8_t> script, uint8_t acks, Step step) {
    if (is_failed()) {
        return fail(ErrorCode::DISABLED, step, __LINE__);
    }
    if (pending()) {
        return fail(ErrorCode::BAD_ARGUMENT, step, __LINE__);
    }
    if (!this->link.write(script)) {
        auto e = mark(make_error(ErrorCode::BUS_ERROR, Step::LIFI_LINK_WRITE, __LINE__), step);
        register_failure(e.error());
        return e;
    }
    this->expected_acks = acks;
    this->chain_step = step;
    // the first ACK cannot come before the whole script has crossed the
    // host UART; the 20 ms reply budget starts after that
    this->deadline_ms = now_ms() + LifiProtocol::frame_time_ms(static_cast<uint32_t>(script.size()),
                                                               LifiProtocol::HOST_BAUD) +
                        LifiProtocol::ACK_TIMEOUT_MS;
    return {};
}

Result<void> LifiTransceiver::ping() {
    reset();
    LifiProtocol::Script<2U> script;
    script.cmd(Cmd::POLLING);
    return submit(script.bytes(), 1U, Step::LIFI_PING);
}

Result<void> LifiTransceiver::fail_chain(const Error& e) noexcept {
    const Step step = this->chain_step;
    this->expected_acks = 0U;
    this->chain_step = Step::NONE;
    auto marked = mark(e, step);
    register_failure(marked.error());
    return marked;
}

Result<void> LifiTransceiver::judge(uint8_t byte) noexcept {
    if (byte == LifiProtocol::ACK) {
        this->expected_acks--;
        if (this->expected_acks == 0U) {
            clear_failures();
        }
        return {};
    }
    if (byte == LifiProtocol::NAK) {
        return fail_chain(make_error(ErrorCode::PROTOCOL_ERROR, Step::LIFI_REPLY, __LINE__));
    }
    if (stray_is_data()) {
        // feed-through: optical bytes and replies share the UART (ICD-008
        // limitation #7); a data byte between two ACKs is not a fault
        on_data(std::span<const uint8_t>(&byte, 1U));
        return {};
    }
    return fail_chain(make_error(ErrorCode::BUS_ERROR, Step::LIFI_REPLY, __LINE__));
}

Result<void> LifiTransceiver::poll() {
    std::array<uint8_t, DRAIN_CHUNK> chunk{};
    for (uint8_t round = 0U; round < DRAIN_ROUNDS; round++) {
        const std::size_t n = this->link.read(chunk);
        if (n == 0U) {
            break;
        }
        std::size_t i = 0U;
        while (i < n && this->expected_acks > 0U) {
            if (auto r = judge(chunk[i]); !r) {
                return r;
            }
            i++;
        }
        if (i < n) {
            on_data(std::span<const uint8_t>(chunk.data() + i, n - i));
        }
    }
    if (this->expected_acks > 0U && static_cast<int32_t>(now_ms() - this->deadline_ms) >= 0) {
        return fail_chain(make_error(ErrorCode::TIMEOUT, Step::LIFI_REPLY, __LINE__));
    }
    return {};
}

// --- LifiSender --------------------------------------------------------------

Result<void> LifiSender::check(uint16_t block_len, uint32_t optical_baud, Step step) noexcept {
    if (block_len == 0U || block_len > LifiProtocol::MAX_BLOCK_LEN) {
        return fail(ErrorCode::BAD_ARGUMENT, step, __LINE__);
    }
    if (optical_baud < LifiProtocol::OPTICAL_BAUD_MIN || optical_baud > LifiProtocol::OPTICAL_BAUD_MAX) {
        return fail(ErrorCode::BAD_ARGUMENT, step, __LINE__);
    }
    return {};
}

Result<void> LifiSender::configure(uint16_t block_len, uint32_t optical_baud) {
    if (auto r = check(block_len, optical_baud, Step::LIFI_CONFIGURE); !r) {
        return r;
    }
    reset();
    LifiProtocol::Script<16U> script;
    script.cmd(Cmd::POLLING).cmd(Cmd::RX_DISABLE).u32(Cmd::SET_OPTICAL_BAUD, optical_baud).u16(Cmd::SET_BUFFER, block_len);
    if (auto r = submit(script.bytes(), CONFIGURE_ACKS, Step::LIFI_CONFIGURE); !r) {
        return r;
    }
    this->len = block_len;
    this->baud = optical_baud;
    this->optical_done_ms = now_ms();
    return {};
}

Result<void> LifiSender::set_rate(uint32_t optical_baud, uint16_t block_len) {
    if (auto r = check(block_len, optical_baud, Step::LIFI_SET_RATE); !r) {
        return r;
    }
    if (!optical_done()) {
        return fail(ErrorCode::BAD_ARGUMENT, Step::LIFI_SET_RATE, __LINE__);
    }
    LifiProtocol::Script<12U> script;
    script.u32(Cmd::SET_OPTICAL_BAUD, optical_baud).u16(Cmd::SET_BUFFER, block_len);
    if (auto r = submit(script.bytes(), SET_RATE_ACKS, Step::LIFI_SET_RATE); !r) {
        return r;
    }
    this->len = block_len;
    this->baud = optical_baud;
    return {};
}

Result<void> LifiSender::load_and_send(std::span<const uint8_t> block) {
    if (block.size() != this->len || this->len == 0U) {
        return fail(ErrorCode::BAD_ARGUMENT, Step::LIFI_LOAD_SEND, __LINE__);
    }
    if (!optical_done()) {
        return fail(ErrorCode::BAD_ARGUMENT, Step::LIFI_LOAD_SEND, __LINE__);
    }
    this->load_script.clear();
    this->load_script.cmd(Cmd::LOAD_BUFFER).raw(block).cmd(Cmd::SEND_BUFFER);
    if (this->load_script.overflowed()) {
        return fail(ErrorCode::BAD_ARGUMENT, Step::LIFI_LOAD_SEND, __LINE__);
    }
    const auto script = this->load_script.bytes();
    if (auto r = submit(script, SEND_ACKS, Step::LIFI_LOAD_SEND); !r) {
        return r;
    }
    this->sent_us = now_us();
    // upload over the host UART, then the optical transmission of the block
    this->optical_done_ms = now_ms() +
                            LifiProtocol::frame_time_ms(static_cast<uint32_t>(script.size()), LifiProtocol::HOST_BAUD) +
                            LifiProtocol::frame_time_ms(this->len, this->baud);
    return {};
}

bool LifiSender::optical_done() const noexcept {
    return !pending() && static_cast<int32_t>(now_ms() - this->optical_done_ms) >= 0;
}

// --- LifiReceiver ------------------------------------------------------------

Result<void> LifiReceiver::check(uint16_t block_len, uint32_t optical_baud, Step step) noexcept {
    if (block_len == 0U || block_len > LifiProtocol::MAX_BLOCK_LEN) {
        return fail(ErrorCode::BAD_ARGUMENT, step, __LINE__);
    }
    if (optical_baud < LifiProtocol::OPTICAL_BAUD_MIN || optical_baud > LifiProtocol::OPTICAL_BAUD_MAX) {
        return fail(ErrorCode::BAD_ARGUMENT, step, __LINE__);
    }
    return {};
}

bool LifiReceiver::stray_is_data() const noexcept {
    return true;
}

Result<void> LifiReceiver::configure(uint16_t block_len, uint32_t optical_baud, uint16_t threshold,
                                     uint8_t cooldown) {
    if (auto r = check(block_len, optical_baud, Step::LIFI_CONFIGURE); !r) {
        return r;
    }
    if (threshold > LifiProtocol::THRESHOLD_MAX) {
        return fail(ErrorCode::BAD_ARGUMENT, Step::LIFI_CONFIGURE, __LINE__);
    }
    reset();
    (void)take();
    LifiProtocol::Script<24U> script;
    script.cmd(Cmd::POLLING)
        .cmd(Cmd::RX_DISABLE)
        .u16(Cmd::SET_BUFFER, block_len)
        .u16(Cmd::SET_THRESHOLD, threshold)
        .u32(Cmd::SET_OPTICAL_BAUD, optical_baud)
        .cmd(Cmd::FEED_THROUGH)
        .cmd(Cmd::RX_ENABLE);
    if (auto r = submit(script.bytes(), CONFIGURE_ACKS, Step::LIFI_CONFIGURE); !r) {
        return r;
    }
    this->len = block_len;
    this->cooldown_ms = cooldown;
    return {};
}

Result<void> LifiReceiver::set_rate(uint32_t optical_baud, uint16_t block_len, uint8_t cooldown) {
    if (auto r = check(block_len, optical_baud, Step::LIFI_SET_RATE); !r) {
        return r;
    }
    LifiProtocol::Script<8U> script;
    script.u32(Cmd::SET_OPTICAL_BAUD, optical_baud).cmd(Cmd::RX_ENABLE);
    if (auto r = submit(script.bytes(), SET_RATE_ACKS, Step::LIFI_SET_RATE); !r) {
        return r;
    }
    this->len = block_len;
    this->cooldown_ms = cooldown;
    return {};
}

void LifiReceiver::on_data(std::span<const uint8_t> bytes) noexcept {
    for (const uint8_t b : bytes) {
        if (this->count < this->len) {
            this->buf[this->count] = b;
            this->count++;
        } else if (this->overflow != 0xFFFFU) {
            this->overflow++;
        }
    }
    this->last_data_ms = uart().last_rx_ms();
    this->last_data_us = uart().last_rx_us();
}

LifiReceiver::Block LifiReceiver::block_state() const noexcept {
    if (this->count == 0U) {
        return Block::EMPTY;
    }
    if (this->count >= this->len) {
        return Block::COMPLETE;
    }
    const uint32_t quiet_ms = now_ms() - this->last_data_ms;
    return quiet_ms >= this->cooldown_ms ? Block::COOLDOWN : Block::FILLING;
}

uint16_t LifiReceiver::take() noexcept {
    const uint16_t had = this->count;
    this->count = 0U;
    this->overflow = 0U;
    return had;
}

LifiReceiver::Ber LifiReceiver::compare(std::span<const uint8_t> expected) const noexcept {
    Ber ber{};
    ber.bits_sent = static_cast<uint16_t>(expected.size() * 8U);
    ber.overflow_bytes = this->overflow;
    ber.first_error_byte = NO_ERROR_BYTE;
    ber.last_error_byte = NO_ERROR_BYTE;

    const std::size_t compared = this->count < expected.size() ? this->count : expected.size();
    for (std::size_t i = 0U; i < compared; i++) {
        const auto diff = static_cast<uint8_t>(this->buf[i] ^ expected[i]);
        if (diff == 0U) {
            continue;
        }
        ber.bit_errors = static_cast<uint16_t>(ber.bit_errors + std::popcount(diff));
        if (ber.first_error_byte == NO_ERROR_BYTE) {
            ber.first_error_byte = static_cast<uint8_t>(i);
        }
        ber.last_error_byte = static_cast<uint8_t>(i);
    }

    if (expected.size() > this->count) {
        ber.missing_bytes = static_cast<uint16_t>(expected.size() - this->count);
        ber.bit_errors = static_cast<uint16_t>(ber.bit_errors + (ber.missing_bytes * 8U));
        if (ber.first_error_byte == NO_ERROR_BYTE) {
            ber.first_error_byte = static_cast<uint8_t>(this->count);
        }
        ber.last_error_byte = static_cast<uint8_t>(expected.size() - 1U);
    }

    ber.first_byte_ok = this->count > 0U && !expected.empty() && this->buf[0] == expected[0];
    return ber;
}
