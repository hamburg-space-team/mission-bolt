#pragma once

#include "can_protocol.hpp"
#include "can_transport.hpp"
#include "node_computer.hpp"
#include "self_test.hpp"
#include <bolt/wire/payloads.hpp>
#include <bolt/wire/types.hpp>

#include <cstdint>
#include <optional>
#include <span>

/// Abstract base for all EXP nodes (D-110: each experiment runs on its own controller).
///
/// Framework flow:
///   on_init()  -> CAN + SD + sensors -> on_experiment_init()
///   on_tick()  -> SYNC -> env/status  -> on_experiment_tick()
///
/// A new EXP only needs to implement:
///   - exp_can_id / exp_env_type / exp_status_type (identity, one-liners)
///   - on_experiment_init()                        (sensor init, optional)
///   - on_experiment_tick()                        (science data, required)
///
/// The concrete EXP .cpp must also declare a file-scope pointer to
/// itself and implement HAL_CAN_RxFifo0MsgPendingCallback that
/// forwards the SYNC frame to notify_sync().
///
/// @ingroup core
class ExpComputer : public NodeComputer {
  public:
    /// Called from the concrete EXP's CAN RX ISR. `mode` rides every SYNC
    /// (no EXP is wired to LO); `test_target` names whose self-test turn it
    /// is, CanProtocol::SELF_TEST_NONE when idle
    void notify_sync(uint16_t tick, BootState::Mode mode, uint8_t test_target) noexcept;

  protected:
    explicit ExpComputer(const Platform& platform, CmsisI2CBus& i2c, Store& storage, CanTransport& can) noexcept;

    // EXP identity
    [[nodiscard]] virtual uint32_t exp_can_id() const noexcept = 0;
    [[nodiscard]] virtual PacketProtocol::PayloadType exp_env_type() const noexcept = 0;
    [[nodiscard]] virtual PacketProtocol::PayloadType exp_status_type() const noexcept = 0;
    [[nodiscard]] virtual PacketProtocol::PayloadType exp_timing_type() const noexcept = 0;
    [[nodiscard]] virtual PacketProtocol::PayloadType exp_test_type() const noexcept = 0;

    /// Origin node for FAULT/GAP/BOOT stamping
    [[nodiscard]] PacketProtocol::NodeId source_node() const noexcept {
        switch (static_cast<uint8_t>(exp_status_type()) >> 4U) {
        case 0x2U:
            return PacketProtocol::NodeId::EXP1;
        case 0x3U:
            return PacketProtocol::NodeId::EXP2;
        case 0x4U:
            return PacketProtocol::NodeId::EXP3;
        default:
            return PacketProtocol::NodeId::UNKNOWN;
        }
    }

    // Packet builder
    virtual void send_env_packet(uint16_t can_tick, uint32_t timestamp_us);
    virtual void send_status_packet(uint16_t can_tick, uint32_t timestamp_us) = 0;
    void send_timing(uint16_t can_tick, uint32_t timestamp_us);

    virtual void on_experiment_init() noexcept {
    }
    /// Flight body: only runs once LO has put the mission in FLIGHT
    virtual void on_experiment_tick(uint16_t can_tick, uint32_t timestamp_us) = 0;

    /// Bench/pre-flight body, runs INSTEAD of on_experiment_tick until LO. The
    /// experiment stays idle here
    virtual void on_test_tick(uint16_t can_tick, uint32_t timestamp_us) {
        (void)can_tick;
        (void)timestamp_us;
    }

    /// Reset all experiment-owned state (counters to 0, LEDs off, in-flight
    /// measurements dropped). Fires on every mission-mode transition so the
    /// new mode starts clean. Device health / recovery state stays untouched
    virtual void on_experiment_reset() noexcept {
    }

    /// This node's self-test step table: common sensor steps first
    /// (test_id 0..2 on every node), board-specific steps after
    [[nodiscard]] virtual std::span<const SelfTest::Step> self_test_steps() const noexcept = 0;

    /// Run aborted from outside (turn moved on mid-run); steps with
    /// actuators override this to park them
    virtual void on_self_test_abort() noexcept {
    }

    /// Mission mode as last broadcast by the BTC. TEST until a SYNC says
    /// otherwise, so an EXP that never hears the BTC never runs its experiment
    [[nodiscard]] BootState::Mode mission_mode() const noexcept {
        return mode;
    }

    void send_gap(uint16_t first_tick, uint8_t count, PacketProtocol::GapReason reason, uint32_t timestamp_us);
    void report_fault(StatusLeds::Fault code, const Error& err) override;

    CanTransport& can; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    static constexpr uint8_t STATUS_INTERVAL = 25U;

  private:
    void on_init() override;
    void on_tick(uint32_t tick_start_us, uint16_t missed_periods) final;
    bool poll_sync(uint16_t& tick_out) noexcept;
    /// SYNC arrived this tick: follow the BTC's mode/target, detect gaps
    void on_sync_received(uint16_t can_tick, uint32_t now_ms, uint32_t tick_start_us);
    void send_timing_packet(uint16_t can_tick, uint32_t timestamp_us);
    /// FLIGHT -> experiment body; TEST -> self-test service + test body
    void dispatch_mode_tick(uint16_t can_tick, uint32_t timestamp_us);
    /// Run one self-test step if the BTC has handed us the turn
    void service_self_test(uint16_t can_tick, uint32_t timestamp_us);
    void send_test_packet(uint16_t can_tick, uint32_t timestamp_us, const SelfTest::Report& report);

    volatile bool sync_pending = false;
    volatile uint16_t sync_tick = 0U;
    volatile BootState::Mode sync_mode = BootState::Mode::TEST;
    volatile uint8_t sync_test_target = CanProtocol::SELF_TEST_NONE;
    BootState::Mode mode = BootState::Mode::TEST;
    uint8_t test_target = CanProtocol::SELF_TEST_NONE;
    SelfTest::Runner tester{*this};
    /// Last report, re-sent every tick until the BTC moves the turn on - a
    /// dropped upstream frame costs one tick, not the run. Cleared when the
    /// turn leaves us, which re-arms the next run
    std::optional<SelfTest::Report> last_report;
    static constexpr uint16_t NO_LAST_TICK = 0xFFFFU;
    static constexpr uint16_t MAX_REPORTABLE_GAP = 250U;
    uint16_t last_can_tick = NO_LAST_TICK;

    // Internal sequencing tick
    uint16_t local_tick = 0U;

    bool gc_done = false;

    static constexpr uint32_t AUTONOMOUS_TIMEOUT_MS = 200U;
    uint32_t last_sync_ms = 0U;
    bool autonomous = false;
};
