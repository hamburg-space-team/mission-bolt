#pragma once

#include "can_protocol.hpp"
#include "exp_computer.hpp"
#include "lifi_pattern.hpp"
#include "lifi_transceiver.hpp"
#include "uart_link.hpp"
#include <bolt/wire/types.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>

/// EXP2 "Bouncy Castle" controller. F-30/F-40: LiFi bit-error rounds
/// between two STM32U031 transceivers (ICD-003, ICD-008).
///
/// One round follows the transceiver team's bench script
/// (tools/TransceiverPyControl/speedtest_simple.py), the sequence that
/// passed on the bench on 2026-10-06: both transceivers in polling mode,
/// the receiver armed with the round's comparator threshold, the sender
/// loaded with the 64-byte PRBS-7 block and triggered, the receiver read
/// back with '\r'. A round spans several ticks; every phase has a tick
/// budget, so a silent transceiver costs a round, never a hang (I-2).
///
/// FLIGHT runs rounds back to back over every FLIGHT_RATES x
/// FLIGHT_THRESHOLDS combination in turn and ships one EXP2_BER per round
/// with the rate index and threshold it used. Self-test step 6 runs the
/// same round at 115 200 over LOOPBACK_THRESHOLDS.
///
/// Link health follows ADR-005 through the two LifiTransceiver devices:
/// FAILED_ROUND_LIMIT rounds in a row ending in a NAK or a missing ACK
/// latch the transceiver that failed (FAULT LIFI_TX / LIFI_RX). Latched
/// transceivers retry on the DeviceBase cooldown.
///
/// @ingroup apps
class Exp2Computer final : public ExpComputer {
  public:
    /// lifi_tx_link / lifi_rx_link: the host UARTs of the sending and the
    /// receiving transceiver, brought up by the board before run()
    explicit Exp2Computer(const Platform& platform, CmsisI2CBus& i2c, Store& storage, CanTransport& can,
                          UartLink& lifi_tx_link, UartLink& lifi_rx_link) noexcept;

  protected:
    void on_experiment_tick(uint16_t can_tick, uint32_t timestamp_us) override;
    void on_experiment_reset() noexcept override;
    void on_self_test_abort() noexcept override;
    void retry_extra_devices() override;

    [[nodiscard]] uint32_t exp_can_id() const noexcept override {
        return CanProtocol::EXP2_DATA_ID;
    }
    [[nodiscard]] PacketProtocol::PayloadType exp_env_type() const noexcept override {
        return PacketProtocol::PayloadType::EXP2_ENV;
    }
    [[nodiscard]] PacketProtocol::PayloadType exp_status_type() const noexcept override {
        return PacketProtocol::PayloadType::EXP2_STATUS;
    }
    [[nodiscard]] PacketProtocol::PayloadType exp_timing_type() const noexcept override {
        return PacketProtocol::PayloadType::EXP2_TIMING;
    }
    [[nodiscard]] PacketProtocol::PayloadType exp_test_type() const noexcept override {
        return PacketProtocol::PayloadType::EXP2_TEST;
    }

    void send_status_packet(uint16_t can_tick, uint32_t timestamp_us) override;
    [[nodiscard]] std::span<const SelfTest::Step> self_test_steps() const noexcept override;

  private:
    static constexpr std::size_t PATTERN_LEN = 64U; // ICD-003 DEFAULT_PATTERN_BYTES

    /// One optical rate of the flight sweep; rate_index is its ICD-003 table index
    struct FlightRate {
        uint32_t baud;
        uint8_t rate_index;
    };
    // ICD-003 rate table entries 0..3; entry 4 (921 600) is above the
    // 460 800 the optical front end follows (ICD-008). Only 115 200 is
    // proven on the bench (2026-10-06)
    static constexpr std::array<FlightRate, 4U> FLIGHT_RATES = {{
        {9600U, 0U},
        {38400U, 1U},
        {115200U, 2U},
        {460800U, 3U},
    }};
    static constexpr uint32_t LOOPBACK_BAUD = 115200U; // the bench script's lowest rate
    // bench 2026-10-06, room light dimmed: each received 64/64 bytes with
    // 0 bit errors; 4000 flooded, 1500 and below received nothing
    static constexpr std::array<uint16_t, 4U> FLIGHT_THRESHOLDS = {{3500U, 3000U, 2500U, 2000U}};
    static constexpr uint8_t FAILED_ROUND_LIMIT = 3U;

    /// Which transceiver ended a round early
    enum class RoundFault : uint8_t {
        NONE, ///< the round ran to the read-back
        TX,   ///< sender NAKed or missed an ACK
        RX,   ///< receiver NAKed or missed an ACK
    };
    struct RoundEnd {
        uint16_t errors; ///< fewest bit errors of any window, NO_REPLY when the round failed
        RoundFault fault;
        ErrorCode code; ///< PROTOCOL_ERROR (NAK) or TIMEOUT; meaningless when fault == NONE
    };
    static constexpr uint16_t NO_REPLY = 0xFFFFU; // worse than any scored block (<= 512)

    /// New seed and PRBS-15 block, then the next rate x threshold combination
    void start_flight_round(uint16_t can_tick, uint32_t timestamp_us);
    void finish_flight_round(const RoundEnd& end);
    void send_ber(const RoundEnd& end);
    /// Up to RX_CHUNKS_PER_TICK EXP2_RX chunks of the finished round
    void send_rx_chunks();
    /// One FAULT per latch; drops the round in flight while a transceiver is out
    void report_lifi_faults() noexcept;

    /// Self-test body shared by both transceivers: '\P', wait for the ACK
    std::optional<PacketProtocol::TestResult> ping_step(LifiTransceiver& dev, bool first, uint32_t& data);
    static std::optional<PacketProtocol::TestResult> step_lifi_tx_ack(NodeComputer& node, bool first,
                                                                      uint32_t& data) noexcept;
    static std::optional<PacketProtocol::TestResult> step_lifi_rx_ack(NodeComputer& node, bool first,
                                                                      uint32_t& data) noexcept;

    /// Flush both host UARTs and send the setup scripts for one round
    void round_start(uint32_t baud, uint16_t threshold);
    /// Advance the round in flight by one tick; a value once it has ended
    [[nodiscard]] std::optional<RoundEnd> round_poll();
    /// SETUP -> SENDING -> SETTLE -> READING transitions of one round
    void round_advance_phase();
    /// The transceiver whose ACKs the current phase is still waiting for
    [[nodiscard]] RoundFault round_waiting_on() const noexcept;
    /// Count ACKs on one host UART; false on a NAK
    [[nodiscard]] static bool count_acks(UartLink& link, uint8_t& acks) noexcept;
    /// READING: everything on the receiver's host UART is '\r' payload
    void round_collect() noexcept;
    /// Start the search for the 64-byte window of the reply with the fewest bit
    /// errors against the pattern (the transceiver adds no framing, ICD-008
    /// limitation #6); a missing byte counts 8
    void round_score_begin() noexcept;
    /// Score up to SCORE_WINDOWS_PER_TICK more windows; true once the search is
    /// done (round_best_errors / round_best_offset final)
    [[nodiscard]] bool round_score_step() noexcept;
    /// Bit errors of the window starting at `offset`; stops counting at `give_up`
    [[nodiscard]] uint16_t round_errors_at(uint16_t offset, uint16_t give_up) const noexcept;
    /// First and last errored byte of the window at `offset`, 0xFF = none
    void round_error_span(uint16_t offset, uint8_t& first, uint8_t& last) const noexcept;

    /// Self-test step 6: one round per LOOPBACK_THRESHOLDS entry, keeps the
    /// best. `data` = (thr << 20) | (count << 10) | errors
    std::optional<PacketProtocol::TestResult> lifi_loopback(bool first, uint32_t& data);
    /// Record `errors` for the current threshold, then start the next round or
    /// report the best threshold found
    std::optional<PacketProtocol::TestResult> loopback_next(uint16_t errors, uint32_t& data);
    static std::optional<PacketProtocol::TestResult> step_lifi_loopback(NodeComputer& node, bool first,
                                                                        uint32_t& data) noexcept;

    LifiSender lifi_tx;
    LifiReceiver lifi_rx;
    // the host UARTs, driven raw by the round engine; lifi_tx / lifi_rx own
    // the same links for the ping self-test and the health latch
    UartLink& raw_tx_link; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    UartLink& raw_rx_link; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

    // FLIGHT
    bool flight_round_active = false;
    uint8_t flight_combo = 0U; ///< rate = combo / thresholds, threshold = combo % thresholds
    uint8_t failed_rounds = 0U;
    uint16_t round_seq = 0U;
    uint32_t seed_state = 0U; ///< xorshift32, seeded from the cycle counter on the first round
    // EXP2_RX: the finished round's bytes, sent before the next round reuses round_buf
    static constexpr uint8_t RX_CHUNK_LEN = 40U; // PayloadExp2Rx::data
    static constexpr uint8_t RX_CHUNKS_PER_TICK = 2U;
    uint8_t rx_chunk_count = 0U;
    uint8_t rx_chunk_next = 0U;
    uint16_t round_tick = 0U;
    uint32_t round_tick_us = 0U;
    // one-shot guards: a latch is reported once, re-armed by the retry
    bool tx_fault_reported = false;
    bool rx_fault_reported = false;

    uint8_t ping_ticks = 0U;

    // round engine, shared by FLIGHT and self-test step 6
    enum class RoundPhase : uint8_t {
        SETUP,   ///< setup scripts out, waiting for the receiver's and sender's ACKs
        SENDING, ///< '\S' out, waiting for its ACK
        SETTLE,  ///< block in the optics
        READING, ///< '\r' out, collecting the reply
        SCORING, ///< searching the reply for the best window, bounded per tick
    };
    static constexpr uint16_t ROUND_LEN = PATTERN_LEN;
    static constexpr uint8_t RX_SETUP_ACKS = 5U;         // \P \f \T \N \R
    static constexpr uint8_t TX_SETUP_ACKS = 3U;         // \P \N \f ('\s' has no reply)
    static constexpr uint16_t PHASE_TICKS = 10U;         // 400 ms per phase, then the round fails
    static constexpr uint16_t SETTLE_TICKS = 3U;         // 120 ms; 64 B take 67 ms at 9600, ~6 ms at 115200
    static constexpr uint16_t READ_TICKS = 4U;           // nothing after 160 ms = empty reply
    static constexpr uint16_t READ_MAX_TICKS = 20U;      // a flood ends within 800 ms
    static constexpr uint16_t ROUND_CAPTURE = 512U;      // '\r' bytes kept; the block may sit behind junk
    static constexpr uint16_t SCORE_WINDOWS_PER_TICK = 64U; // ~9 ms in the Debug image, a full flood in 7 ticks
    RoundPhase round_phase = RoundPhase::SETUP;
    uint16_t round_ticks = 0U;
    uint8_t round_rx_acks = 0U;
    uint8_t round_tx_acks = 0U;
    uint16_t round_threshold = 0U;
    uint8_t round_rate_index = 0U;
    uint8_t round_pattern_code = 0U; ///< ICD-003 pattern code of round_pattern
    uint16_t round_seed = 0U;
    std::array<uint8_t, ROUND_LEN> round_pattern{}; ///< the block this round sends
    uint32_t round_send_us = 0U;          ///< tick_us when '\S' was handed to the UART
    uint32_t round_ack_latency_us = 0U;   ///< '\S' -> the sender's ACK, from the interrupt stamp
    std::array<uint8_t, ROUND_CAPTURE> round_buf{};
    uint16_t round_count = 0U;            ///< bytes the '\r' reply carried
    uint16_t round_best_offset = 0U;      ///< offset of the best-matching 64 B window
    uint16_t round_best_errors = 0U;      ///< bit errors of that window
    uint16_t round_score_next = 0U;       ///< next window offset to score
    uint16_t round_score_last = 0U;       ///< last window offset that fits the capture
    uint8_t round_quiet_ticks = 0U;       ///< consecutive READING ticks without a byte

    // self-test step 6: speedtest_simple.py's threshold list, so the result
    // compares with the bench
    static constexpr std::array<uint16_t, 8U> LOOPBACK_THRESHOLDS = {{4000U, 3500U, 3000U, 2500U,
                                                                      2000U, 1500U, 1000U, 800U}};
    uint8_t sweep_idx = 0U;            ///< threshold currently under test
    uint16_t sweep_best_err = 0xFFFFU; ///< fewest bit errors seen so far
    uint16_t sweep_best_thr = 0U;      ///< threshold that produced sweep_best_err
    uint16_t sweep_best_count = 0U;    ///< bytes received in that round
    // per-threshold results, kept in RAM for bench inspection (tools/lifi-buffer.py)
    std::array<uint16_t, 8U> sweep_count_at{};  ///< bytes the '\r' reply carried
    std::array<uint16_t, 8U> sweep_err_at{};    ///< fewest bit errors over all offsets
    std::array<uint16_t, 8U> sweep_offset_at{}; ///< offset of the best-matching 64 B window
};
