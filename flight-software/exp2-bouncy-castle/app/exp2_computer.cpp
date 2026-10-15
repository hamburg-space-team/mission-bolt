#include "exp2_computer.hpp"
#include "can_protocol.hpp"
#include "main.h" // IWYU pragma: keep
#include "wcet.hpp"
#include <bolt/wire/payloads.hpp>
#include <bolt/wire/selftest.hpp>

#include <array>
#include <bit>

extern CAN_HandleTypeDef hcan1;

namespace {
    Exp2Computer* instance_g = nullptr;
}
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* hcan) {
    CAN_RxHeaderTypeDef hdr{};

    std::array<uint8_t, 8U> data{};
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &hdr, data.data()) != HAL_OK) {
        return;
    }
    if (hdr.StdId == CanProtocol::SYNC_ID && hdr.DLC >= CanProtocol::SYNC_DLC && instance_g != nullptr) {
        const auto tick = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8U);
        // data[2] = mission mode, data[3] = self-test target; the DLC check
        // above guarantees both are present
        const auto mode = (data[2] == static_cast<uint8_t>(BootState::Mode::FLIGHT)) ? BootState::Mode::FLIGHT
                                                                                     : BootState::Mode::TEST;
        instance_g->notify_sync(tick, mode, data[3]);
    }
}

Exp2Computer::Exp2Computer(const Platform& platform, CmsisI2CBus& i2c, Store& storage, CanTransport& can,
                           UartLink& lifi_tx_link, UartLink& lifi_rx_link) noexcept
    : ExpComputer(platform, i2c, storage, can), lifi_tx(lifi_tx_link, platform.tick_ms, platform.tick_us),
      lifi_rx(lifi_rx_link, platform.tick_ms, platform.tick_us), raw_tx_link(lifi_tx_link),
      raw_rx_link(lifi_rx_link) {
    instance_g = this;
}

// --- Flight tick --------------------------------------------------------------

void Exp2Computer::on_experiment_tick(uint16_t can_tick, uint32_t timestamp_us) {
    report_lifi_faults();
    if (this->lifi_tx.is_failed() || this->lifi_rx.is_failed()) {
        return; // retry_extra_devices() restarts the rounds after the cooldown
    }
    if (this->rx_chunk_next < this->rx_chunk_count) {
        send_rx_chunks(); // the next round reuses round_buf, so the chunks go first
        return;
    }
    if (!this->flight_round_active) {
        start_flight_round(can_tick, timestamp_us);
        return;
    }
    if (const auto end = round_poll()) {
        finish_flight_round(*end);
    }
}

void Exp2Computer::start_flight_round(uint16_t can_tick, uint32_t timestamp_us) {
    BOLT_TIME(this->timings, CFG);
    // xorshift32: a fresh seed per round; the seed travels in EXP2_BER, so
    // the ground never needs this generator, only the PRBS-15
    if (this->seed_state == 0U) {
        this->seed_state = ErrorClock::now_cycles() | 1U; // boot-to-boot variety, never 0
    }
    this->seed_state ^= this->seed_state << 13U;
    this->seed_state ^= this->seed_state >> 17U;
    this->seed_state ^= this->seed_state << 5U;
    this->round_seed = LifiPattern::prbs15_seed(static_cast<uint16_t>(this->seed_state));
    this->round_pattern_code = static_cast<uint8_t>(LifiPattern::Kind::PRBS15);
    LifiPattern::fill_prbs15(this->round_pattern, this->round_seed);

    const FlightRate& rate = FLIGHT_RATES[this->flight_combo / FLIGHT_THRESHOLDS.size()];
    round_start(rate.baud, FLIGHT_THRESHOLDS[this->flight_combo % FLIGHT_THRESHOLDS.size()]);
    this->round_rate_index = rate.rate_index;
    this->round_seq++;
    this->flight_round_active = true;
    this->round_tick = can_tick;
    this->round_tick_us = timestamp_us;
}

void Exp2Computer::report_lifi_faults() noexcept {
    if (this->lifi_tx.is_failed() && !this->tx_fault_reported) {
        report_fault(StatusLeds::Fault::LIFI_TX, this->lifi_tx.last_error());
        this->tx_fault_reported = true;
    }
    if (this->lifi_rx.is_failed() && !this->rx_fault_reported) {
        report_fault(StatusLeds::Fault::LIFI_RX, this->lifi_rx.last_error());
        this->rx_fault_reported = true;
    }
    if (this->lifi_tx.is_failed() || this->lifi_rx.is_failed()) {
        this->flight_round_active = false;
    }
}

void Exp2Computer::finish_flight_round(const RoundEnd& end) {
    send_ber(end);
    this->flight_round_active = false;
    const uint16_t kept = (end.fault != RoundFault::NONE)        ? 0U
                          : (this->round_count < ROUND_CAPTURE) ? this->round_count
                                                                : ROUND_CAPTURE;
    this->rx_chunk_count = static_cast<uint8_t>((kept + RX_CHUNK_LEN - 1U) / RX_CHUNK_LEN);
    this->rx_chunk_next = 0U;
    this->flight_combo =
        static_cast<uint8_t>((this->flight_combo + 1U) % (FLIGHT_RATES.size() * FLIGHT_THRESHOLDS.size()));
    if (end.fault == RoundFault::NONE) {
        // an empty reply is a dark round, not a link fault: it ships as an
        // invalid measurement and the next round tries again
        this->failed_rounds = 0U;
        return;
    }
    this->failed_rounds++;
    if (this->failed_rounds < FAILED_ROUND_LIMIT) {
        return;
    }
    this->failed_rounds = 0U;
    LifiTransceiver& culprit = (end.fault == RoundFault::TX) ? static_cast<LifiTransceiver&>(this->lifi_tx)
                                                              : static_cast<LifiTransceiver&>(this->lifi_rx);
    culprit.disable(make_error(end.code, Step::LIFI_ROUND));
}

void Exp2Computer::send_ber(const RoundEnd& end) {
    using namespace PacketProtocol;
    BOLT_TIME(this->timings, SEND);

    PayloadExp2Ber p{};
    p.rate_index = this->round_rate_index;
    p.threshold = this->round_threshold;
    p.round_seq = this->round_seq;
    p.pattern = this->round_pattern_code;
    p.seed = this->round_seed;
    p.rx_count = (end.fault == RoundFault::NONE) ? this->round_count : 0U;
    p.first_error_byte = LifiReceiver::NO_ERROR_BYTE;
    p.last_error_byte = LifiReceiver::NO_ERROR_BYTE;
    // I-6: a failed round or an empty reply carries no measurement
    const bool valid = (end.fault == RoundFault::NONE) && (this->round_count != 0U);
    p.measurement_valid = valid ? 1U : 0U;
    if (valid) {
        // recv = send + the wrap-safe interval, so the ground's recv - send is
        // the '\S' -> ACK latency even when the us clock wrapped in between
        p.timestamp_send_us = this->round_send_us;
        p.timestamp_recv_us = this->round_send_us + this->round_ack_latency_us;
        p.bits_sent = ROUND_LEN * 8U;
        p.bit_errors = end.errors;
        round_error_span(this->round_best_offset, p.first_error_byte, p.last_error_byte);
        p.block_offset = this->round_best_offset;
    }

    if (auto len = this->pkt.build(this->tx_buf.data(), PayloadType::EXP2_BER, Tick{this->round_tick},
                                   TimestampUs{this->round_tick_us}, &p, static_cast<uint8_t>(sizeof(p)))) {
        this->can.send(exp_can_id(), this->tx_buf.data(), *len);
        (void)this->storage.write(this->tx_buf.data(), *len);
    }
}

void Exp2Computer::send_rx_chunks() {
    using namespace PacketProtocol;
    BOLT_TIME(this->timings, SEND);
    const uint16_t kept = (this->round_count < ROUND_CAPTURE) ? this->round_count : ROUND_CAPTURE;
    for (uint8_t n = 0U; n < RX_CHUNKS_PER_TICK && this->rx_chunk_next < this->rx_chunk_count; n++) {
        PayloadExp2Rx p{};
        p.round_seq = this->round_seq;
        p.chunk_index = this->rx_chunk_next;
        p.chunk_count = this->rx_chunk_count;
        const auto start = static_cast<uint16_t>(this->rx_chunk_next * RX_CHUNK_LEN);
        const auto left = static_cast<uint16_t>(kept - start);
        p.length = static_cast<uint8_t>((left < RX_CHUNK_LEN) ? left : RX_CHUNK_LEN);
        for (uint8_t i = 0U; i < p.length; i++) {
            p.data[i] = this->round_buf[start + i]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }
        if (auto len = this->pkt.build(this->tx_buf.data(), PayloadType::EXP2_RX, Tick{this->round_tick},
                                       TimestampUs{this->round_tick_us}, &p, static_cast<uint8_t>(sizeof(p)))) {
            this->can.send(exp_can_id(), this->tx_buf.data(), *len);
            (void)this->storage.write(this->tx_buf.data(), *len);
        }
        this->rx_chunk_next++;
    }
}

// --- Framework hooks ----------------------------------------------------------

void Exp2Computer::on_experiment_reset() noexcept {
    // the new mode starts from the first threshold; a round in flight is
    // dropped. Device health stays (ADR-005)
    this->flight_round_active = false;
    this->flight_combo = 0U;
    this->rx_chunk_count = 0U;
    this->rx_chunk_next = 0U;
    this->failed_rounds = 0U;
}

void Exp2Computer::retry_extra_devices() {
    bool retried = false;
    if (this->lifi_tx.retry_due()) {
        this->lifi_tx.clear_latch();
        this->tx_fault_reported = false;
        retried = true;
    }
    if (this->lifi_rx.retry_due()) {
        this->lifi_rx.clear_latch();
        this->rx_fault_reported = false;
        retried = true;
    }
    if (retried) {
        this->failed_rounds = 0U;
        this->flight_round_active = false; // the next tick starts a fresh round
    }
}

void Exp2Computer::send_status_packet(uint16_t can_tick, uint32_t timestamp_us) {
    using namespace PacketProtocol;
    PayloadExpStatus status{};
    status.uptime_s = this->platform.tick_ms() / 1000U;
    status.sd_status = static_cast<uint8_t>(this->storage.is_mounted() ? 0x01U : 0x00U);
    status.mode = static_cast<MissionMode>(mission_mode());

    if (auto len = this->pkt.build(this->tx_buf.data(), exp_status_type(), Tick{can_tick}, TimestampUs{timestamp_us},
                                   &status, static_cast<uint8_t>(sizeof(status)))) {
        this->can.send(exp_can_id(), this->tx_buf.data(), *len);
        (void)this->storage.write(this->tx_buf.data(), *len);
    }
}

// --- Self-test ----------------------------------------------------------------

void Exp2Computer::on_self_test_abort() noexcept {
    // an interrupted probe leaves a chain outstanding, the next probe must
    // start from reset(); an interrupted round is simply abandoned, the next
    // one flushes both links first
    this->lifi_tx.reset();
    this->lifi_rx.reset();
    this->flight_round_active = false;
}

std::optional<PacketProtocol::TestResult> Exp2Computer::ping_step(LifiTransceiver& dev, bool first, uint32_t& data) {
    using PacketProtocol::TestResult;
    if (dev.is_failed()) {
        return TestResult::SKIPPED;
    }
    if (first) {
        this->ping_ticks = 0U;
        return dev.ping() ? std::nullopt : std::optional{TestResult::FAIL};
    }
    this->ping_ticks++;
    if (!dev.poll()) {
        return TestResult::FAIL; // NAK, stray byte or 20 ms of silence
    }
    if (dev.pending()) {
        return std::nullopt;
    }
    data = this->ping_ticks;
    return TestResult::PASS;
}

std::optional<PacketProtocol::TestResult> Exp2Computer::step_lifi_tx_ack(NodeComputer& node, bool first,
                                                                         uint32_t& data) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast) - RTTI is off
    auto& self = static_cast<Exp2Computer&>(node);
    return self.ping_step(self.lifi_tx, first, data);
}

std::optional<PacketProtocol::TestResult> Exp2Computer::step_lifi_rx_ack(NodeComputer& node, bool first,
                                                                         uint32_t& data) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast) - RTTI is off
    auto& self = static_cast<Exp2Computer&>(node);
    return self.ping_step(self.lifi_rx, first, data);
}

// --- Round engine -------------------------------------------------------------

void Exp2Computer::round_start(uint32_t baud, uint16_t threshold) {
    using LifiProtocol::Cmd;
    this->raw_tx_link.flush_rx();
    this->raw_rx_link.flush_rx();

    // receiver: the bench script's \P \f \T \R, plus \N right before \R so a
    // repeated round restarts the read position (ICD-008 limitation #2)
    LifiProtocol::Script<20U> rx;
    rx.cmd(Cmd::POLLING)
        .u32(Cmd::SET_OPTICAL_BAUD, baud)
        .u16(Cmd::SET_THRESHOLD, threshold)
        .u16(Cmd::SET_BUFFER, ROUND_LEN)
        .cmd(Cmd::RX_ENABLE);
    // sender, in the bench script's order: \P \N <len> \s <block> \f <baud>
    LifiProtocol::Script<16U + ROUND_LEN> tx;
    tx.cmd(Cmd::POLLING)
        .u16(Cmd::SET_BUFFER, ROUND_LEN)
        .cmd(Cmd::LOAD_BUFFER)
        .raw(this->round_pattern)
        .u32(Cmd::SET_OPTICAL_BAUD, baud);

    (void)this->raw_rx_link.write(rx.bytes());
    (void)this->raw_tx_link.write(tx.bytes());
    this->round_threshold = threshold;
    this->round_rx_acks = 0U;
    this->round_tx_acks = 0U;
    this->round_count = 0U;
    this->round_best_offset = 0U;
    this->round_ack_latency_us = 0U;
    this->round_phase = RoundPhase::SETUP;
    this->round_ticks = 0U;
}

std::optional<Exp2Computer::RoundEnd> Exp2Computer::round_poll() {
    BOLT_TIME(this->timings, READ);
    this->round_ticks++;
    if (this->round_phase == RoundPhase::READING) {
        round_collect();
        // read until the line goes quiet, so a flood cannot spill into the next round
        const bool empty_reply = (this->round_count == 0U) && (this->round_ticks >= READ_TICKS);
        const bool drained = (this->round_count != 0U) && (this->round_quiet_ticks >= 2U);
        if (!empty_reply && !drained && this->round_ticks < READ_MAX_TICKS) {
            return std::nullopt;
        }
        round_score_begin();
    }
    if (this->round_phase == RoundPhase::SCORING) {
        if (!round_score_step()) {
            return std::nullopt;
        }
        return RoundEnd{this->round_best_errors, RoundFault::NONE, ErrorCode::TIMEOUT};
    }

    if (!count_acks(this->raw_tx_link, this->round_tx_acks)) {
        return RoundEnd{NO_REPLY, RoundFault::TX, ErrorCode::PROTOCOL_ERROR};
    }
    if (!count_acks(this->raw_rx_link, this->round_rx_acks)) {
        return RoundEnd{NO_REPLY, RoundFault::RX, ErrorCode::PROTOCOL_ERROR};
    }
    if (this->round_ticks > PHASE_TICKS) {
        return RoundEnd{NO_REPLY, round_waiting_on(), ErrorCode::TIMEOUT};
    }
    round_advance_phase();
    return std::nullopt;
}

Exp2Computer::RoundFault Exp2Computer::round_waiting_on() const noexcept {
    if (this->round_phase == RoundPhase::SETUP && this->round_rx_acks < RX_SETUP_ACKS) {
        return RoundFault::RX;
    }
    return RoundFault::TX; // SETUP with the receiver done, or SENDING: the sender owes the ACK
}

bool Exp2Computer::count_acks(UartLink& link, uint8_t& acks) noexcept {
    std::array<uint8_t, 32U> chunk{};
    for (uint8_t round = 0U; round < 8U; round++) { // bounded: 256 B per tick
        const std::size_t n = link.read(chunk);
        for (std::size_t i = 0U; i < n; i++) {
            if (chunk[i] == LifiProtocol::NAK) {
                return false;
            }
            if (chunk[i] == LifiProtocol::ACK && acks != 0xFFU) {
                acks++;
            }
        }
        if (n == 0U) {
            break;
        }
    }
    return true;
}

void Exp2Computer::round_collect() noexcept {
    std::array<uint8_t, 32U> chunk{};
    bool any = false;
    for (uint8_t round = 0U; round < 16U; round++) { // bounded: 512 B per tick, one ring's worth
        const std::size_t n = this->raw_rx_link.read(chunk);
        any = any || (n != 0U);
        for (std::size_t i = 0U; i < n; i++) {
            if (this->round_count < ROUND_CAPTURE) {
                this->round_buf[this->round_count] = chunk[i];
            }
            if (this->round_count != 0xFFFFU) {
                this->round_count++;
            }
        }
        if (n == 0U) {
            break;
        }
    }
    this->round_quiet_ticks = any ? 0U : static_cast<uint8_t>(this->round_quiet_ticks + 1U);
}

uint16_t Exp2Computer::round_errors_at(uint16_t offset, uint16_t give_up) const noexcept {
    const uint16_t stored = (this->round_count < ROUND_CAPTURE) ? this->round_count : ROUND_CAPTURE;
    uint16_t errors = 0U;
    for (uint16_t i = 0U; i < ROUND_LEN && errors < give_up; i++) {
        const auto at = static_cast<uint16_t>(offset + i);
        if (at >= stored) {
            errors = static_cast<uint16_t>(errors + 8U); // never arrived
        } else {
            const auto diff = static_cast<uint8_t>(this->round_buf[at] ^ this->round_pattern[i]);
            errors = static_cast<uint16_t>(errors + std::popcount(diff));
        }
    }
    return errors;
}

void Exp2Computer::round_error_span(uint16_t offset, uint8_t& first, uint8_t& last) const noexcept {
    const uint16_t stored = (this->round_count < ROUND_CAPTURE) ? this->round_count : ROUND_CAPTURE;
    first = LifiReceiver::NO_ERROR_BYTE;
    last = LifiReceiver::NO_ERROR_BYTE;
    for (uint16_t i = 0U; i < ROUND_LEN; i++) {
        const auto at = static_cast<uint16_t>(offset + i);
        if (at >= stored || this->round_buf[at] != this->round_pattern[i]) {
            if (first == LifiReceiver::NO_ERROR_BYTE) {
                first = static_cast<uint8_t>(i);
            }
            last = static_cast<uint8_t>(i);
        }
    }
}

void Exp2Computer::round_score_begin() noexcept {
    const uint16_t stored = (this->round_count < ROUND_CAPTURE) ? this->round_count : ROUND_CAPTURE;
    this->round_score_last = (stored > ROUND_LEN) ? static_cast<uint16_t>(stored - ROUND_LEN) : 0U;
    this->round_best_errors = round_errors_at(0U, NO_REPLY);
    this->round_best_offset = 0U;
    this->round_score_next = 1U;
    this->round_phase = RoundPhase::SCORING;
}

bool Exp2Computer::round_score_step() noexcept {
    // a 3936-byte flood leaves 449 windows; all of them in one tick broke
    // the 40 ms budget (I-1: 61 ms measured in the Debug image)
    for (uint16_t n = 0U; n < SCORE_WINDOWS_PER_TICK; n++) {
        if (this->round_best_errors == 0U || this->round_score_next > this->round_score_last) {
            return true;
        }
        const uint16_t e = round_errors_at(this->round_score_next, this->round_best_errors);
        if (e < this->round_best_errors) {
            this->round_best_errors = e;
            this->round_best_offset = this->round_score_next;
        }
        this->round_score_next++;
    }
    return this->round_best_errors == 0U || this->round_score_next > this->round_score_last;
}

void Exp2Computer::round_advance_phase() {
    using LifiProtocol::Cmd;
    switch (this->round_phase) {
    case RoundPhase::SETUP:
        if (this->round_rx_acks >= RX_SETUP_ACKS && this->round_tx_acks >= TX_SETUP_ACKS) {
            LifiProtocol::Script<2U> send;
            send.cmd(Cmd::SEND_BUFFER);
            this->round_send_us = this->platform.tick_us(); // before the UART can start
            (void)this->raw_tx_link.write(send.bytes());
            this->round_tx_acks = 0U;
            this->round_phase = RoundPhase::SENDING;
            this->round_ticks = 0U;
        }
        break;
    case RoundPhase::SENDING:
        if (this->round_tx_acks >= 1U) {
            // the ACK is the only byte the sender sends after '\S'; its
            // interrupt stamp is the arrival, not this tick's poll
            this->round_ack_latency_us = ErrorClock::us_elapsed(this->round_send_us, this->raw_tx_link.last_rx_us());
            this->round_phase = RoundPhase::SETTLE;
            this->round_ticks = 0U;
        }
        break;
    case RoundPhase::SETTLE:
        if (this->round_ticks >= SETTLE_TICKS) {
            this->raw_rx_link.flush_rx();
            LifiProtocol::Script<2U> read;
            read.cmd(Cmd::READ_RX);
            (void)this->raw_rx_link.write(read.bytes());
            this->round_count = 0U;
            this->round_quiet_ticks = 0U;
            this->round_phase = RoundPhase::READING;
            this->round_ticks = 0U;
        }
        break;
    case RoundPhase::READING:
    case RoundPhase::SCORING:
        break;
    }
}

// --- Self-test step 6 -----------------------------------------------------------

std::optional<PacketProtocol::TestResult> Exp2Computer::lifi_loopback(bool first, uint32_t& data) {
    using PacketProtocol::TestResult;
    if (first) {
        if (this->lifi_tx.is_failed() || this->lifi_rx.is_failed()) {
            return TestResult::SKIPPED;
        }
        this->sweep_idx = 0U;
        this->sweep_best_err = 0xFFFFU;
        this->sweep_best_thr = 0U;
        this->sweep_best_count = 0U;
        this->sweep_count_at.fill(0U);
        this->sweep_err_at.fill(0xFFFFU);
        this->sweep_offset_at.fill(0U);
        // the fixed PRBS-7 block tools/lifi-buffer.py compares against
        LifiPattern::fill(this->round_pattern, LifiPattern::Kind::PRBS7);
        round_start(LOOPBACK_BAUD, LOOPBACK_THRESHOLDS[0]);
        return std::nullopt;
    }
    if (const auto end = round_poll()) {
        return loopback_next(end->errors, data); // a NAK or silence scores this threshold worst
    }
    return std::nullopt;
}

std::optional<PacketProtocol::TestResult> Exp2Computer::loopback_next(uint16_t errors, uint32_t& data) {
    using PacketProtocol::TestResult;
    this->sweep_count_at[this->sweep_idx] = this->round_count;
    this->sweep_err_at[this->sweep_idx] = errors;
    this->sweep_offset_at[this->sweep_idx] = this->round_best_offset;
    if (errors < this->sweep_best_err) {
        this->sweep_best_err = errors;
        this->sweep_best_thr = LOOPBACK_THRESHOLDS[this->sweep_idx];
        this->sweep_best_count = this->round_count;
    }
    if ((this->sweep_idx + 1U) < LOOPBACK_THRESHOLDS.size()) {
        this->sweep_idx++;
        round_start(LOOPBACK_BAUD, LOOPBACK_THRESHOLDS[this->sweep_idx]);
        return std::nullopt;
    }
    const uint32_t thr = this->sweep_best_thr & 0xFFFU;
    const uint32_t count = (this->sweep_best_count > 1023U) ? 1023U : this->sweep_best_count;
    const uint32_t err = (this->sweep_best_err > 1023U) ? 1023U : this->sweep_best_err;
    data = (thr << 20U) | (count << 10U) | err;
    return this->sweep_best_err == 0U ? TestResult::PASS : TestResult::FAIL;
}

std::optional<PacketProtocol::TestResult> Exp2Computer::step_lifi_loopback(NodeComputer& node, bool first,
                                                                           uint32_t& data) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast) - RTTI is off
    auto& self = static_cast<Exp2Computer&>(node);
    return self.lifi_loopback(first, data);
}

std::span<const SelfTest::Step> Exp2Computer::self_test_steps() const noexcept {
    static constexpr std::array<SelfTest::Step, 7U> steps = {{
        {&NodeComputer::step_tmp_whoami},    // 0: TMP117 device ID
        {&NodeComputer::step_tmp_read},      // 1: TMP117 raw temperature
        {&NodeComputer::step_baro_prom},     // 2: MS5611 PROM CRC + C1
        {&NodeComputer::step_sd_mounted},    // 3: SD mounted
        {&Exp2Computer::step_lifi_tx_ack},   // 4: sender transceiver answers
        {&Exp2Computer::step_lifi_rx_ack},   // 5: receiver transceiver answers
        {&Exp2Computer::step_lifi_loopback}, // 6: end-to-end LiFi round, bit-perfect
    }};
    static_assert(steps.size() == PacketProtocol::EXP2_SELF_TEST_COUNT,
                  "table drifted from the bolt/wire/selftest.hpp contract");
    return steps;
}
