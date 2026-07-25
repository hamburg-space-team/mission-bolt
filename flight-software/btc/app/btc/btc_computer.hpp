#pragma once

#include "Driver_USART.h"
#include "board/btc_board.hpp"
#include "board/rs422_downlink.hpp"
#include "can_reassembler.hpp"
#include "imu_supervisor.hpp"
#include "lo_tracker.hpp"
#include "node_computer.hpp"
#include "protocol/uplink_parser.hpp"
#include "self_test.hpp"
#include "self_test_sequencer.hpp"
#include "telemetry_emitter.hpp"
#include <bolt/wire/types.hpp>

#include <cstdint>

/// @defgroup apps Applications

/// BTC master controller.
///
///   - Emits the 25 Hz CAN SYNC frame that drives the EXPs.
///   - Reacts to LO/SOE/SODS REXUS signals via EXTI (F-00).
///   - Reassembles EXP CAN frames and forwards them onto the RS-422
///     downlink (D-100 central calculations).
///   - Mirrors the EXP downlink to its own SD log (D-130).
///   - Produces its own PayloadBtcEnv and PayloadBtcStatus payloads.
///
/// @ingroup apps
class BtcComputer final : public NodeComputer {
  public:
    explicit BtcComputer(const Platform& platform, CmsisI2CBus& i2c, Store& storage, ARM_DRIVER_USART& usart,
                         BtcBoard& board) noexcept;

    /// Forwards the raw bxCAN frame into the reassembler.
    void notify_can_frame(uint32_t can_id, const uint8_t* raw) noexcept;

    /// Latch that the LO EXTI edge fired; consumed once in on_tick.
    void notify_lo_edge() noexcept;

    /// Latch an IMU DATA_READY edge with its capture timestamp
    void notify_imu_drdy(uint32_t timestamp_us) noexcept;

  protected:
    void on_init() override;
    void on_tick(uint32_t tick_start_us, uint16_t missed_periods) override;
    void on_drain(uint32_t deadline_ms) override;
    void report_fault(StatusLeds::Fault code, const Error& err) override;
    void init_extra_sensors() override;
    void retry_extra_devices() override;

  private:
    static constexpr uint8_t SAVE_INTERVAL = 25U;
    static constexpr uint32_t DOWNLINK_BAUD = 38400U;

    Rs422Downlink downlink;
    CanReassembler reassembler;
    BtcBoard& board; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    PacketProtocol::UplinkParser uplink;
    TelemetryEmitter emitter;
    LoTracker lo;
    ImuSupervisor imu_sup;
    SelfTestSequencer sequencer;
    SelfTest::Runner tester{*this};
    // TEST until LO; recovered from .noinit on a warm reset so a reset in
    // flight cannot drop the mission back into TEST
    BootState::Mode mode = BootState::Mode::TEST;
    uint16_t sync_count = 0U;
    bool gc_done = false;

    // Set from ISR context via the notify_* methods, read in the main loop.
    volatile bool lo_pending = false;
    volatile bool imu_drdy = false;
    volatile uint32_t imu_drdy_ts = 0U;

    void send_gap_to_uart(uint16_t first_tick, uint8_t count, PacketProtocol::GapReason reason, uint32_t timestamp_us);
    void poll_uplink(uint32_t timestamp_us);
    void handle_uplink(uint8_t opcode, uint8_t seq, uint32_t timestamp_us);
    void send_env_packet(uint32_t tick_start_us);
    void send_status_packet(uint32_t tick_start_us);
    void send_imu_packet(uint32_t timestamp_us);
    void send_timing(uint32_t timestamp_us);
    void send_test_packet(uint32_t timestamp_us, const SelfTest::Report& report);
    /// Step table: common sensors (test_id 0..2), then the board IMU (3..4)
    [[nodiscard]] static std::span<const SelfTest::Step> self_test_steps() noexcept;
    static std::optional<PacketProtocol::TestResult> step_imu_whoami(NodeComputer& node, bool first,
                                                                     uint32_t& data) noexcept;
    static std::optional<PacketProtocol::TestResult> step_imu_read(NodeComputer& node, bool first,
                                                                   uint32_t& data) noexcept;
    /// Switch the mission mode and persist it. LO / START_EXPERIMENT enter
    /// FLIGHT; STOP_EXPERIMENT returns to TEST, but only before LO
    void set_mode(BootState::Mode next);
    /// Bench-only tick body: steps the BTC's own self-test while the
    /// sequencer says it is our turn (EXPs are driven via SYNC instead)
    void on_test_tick(uint32_t tick_start_us);
};
