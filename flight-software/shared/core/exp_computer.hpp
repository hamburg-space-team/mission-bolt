#pragma once

#include "can_transport.hpp"
#include "node_computer.hpp"
#include <bolt/wire/payloads.hpp>
#include <bolt/wire/types.hpp>

#include <cstdint>

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
    /// Called by HAL_CAN_RxFifo0MsgPendingCallback in each concrete
    /// EXP's .cpp.
    void notify_sync(uint16_t tick) noexcept;

  protected:
    explicit ExpComputer(const Platform& platform, CmsisI2CBus& i2c, Store& storage, CanTransport& can) noexcept;

    // EXP identity - one-liner overrides in each concrete subclass
    [[nodiscard]] virtual uint32_t exp_can_id() const noexcept = 0;
    [[nodiscard]] virtual PacketProtocol::PayloadType exp_env_type() const noexcept = 0;
    [[nodiscard]] virtual PacketProtocol::PayloadType exp_status_type() const noexcept = 0;

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

    // Packet builders - EXP3 overrides send_env_packet() for PayloadExp3Env
    virtual void send_env_packet(uint16_t can_tick, uint32_t timestamp_us);
    virtual void send_status_packet(uint16_t can_tick, uint32_t timestamp_us);
    // Downlink this node's worst-case scope timings (source-tagged via source_node()).
    void send_timing(uint16_t can_tick, uint32_t timestamp_us);

    /// Board-specific extras for the status payload (EXP1: transient
    /// spectrometer failure counters). Base leaves the fields at 0.
    virtual void fill_status(PacketProtocol::PayloadExpStatus& status) noexcept {
        (void)status;
    }

    virtual void on_experiment_init() noexcept {
    }
    virtual void on_experiment_tick(uint16_t can_tick, uint32_t timestamp_us) = 0;

    void send_gap(uint16_t first_tick, uint8_t count, PacketProtocol::GapReason reason, uint32_t timestamp_us);
    /// Latches the LED code and ships a FAULT packet (error step trace,
    /// ADR-012) over CAN + SD.
    void report_fault(StatusLeds::Fault code, const Error& err) override;

    CanTransport& can; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    static constexpr uint8_t STATUS_INTERVAL = 25U;

  private:
    void on_init() override;
    void on_tick(uint32_t tick_start_us, uint16_t missed_periods) final;
    bool poll_sync(uint16_t& tick_out) noexcept;
    // WCET window bookkeeping + periodic env/status/flush, split out of on_tick.
    void roll_timing_window(uint16_t can_tick, uint32_t timestamp_us);
    void send_periodic(uint16_t can_tick, uint32_t timestamp_us);

    volatile bool sync_pending = false;
    volatile uint16_t sync_tick = 0U;
    static constexpr uint16_t NO_LAST_TICK = 0xFFFFU;
    // Largest forward SYNC hole reported as a gap (also the count field's
    // uint8 ceiling). Larger differences in wrap arithmetic mean the master
    // restarted its counter (LO reset) -> re-baseline without a gap.
    static constexpr uint16_t MAX_REPORTABLE_GAP = 250U;
    uint16_t last_can_tick = NO_LAST_TICK;

    // Internal sequencing tick: +1 per executed tick, gap-free by
    // construction.
    uint16_t local_tick = 0U;
    bool gc_done = false;

    // Autonomous fallback: if no CAN SYNC arrives for this long, the node
    // stops waiting and drives the experiment on a self-generated tick.
    static constexpr uint32_t AUTONOMOUS_TIMEOUT_MS = 200U;
    uint32_t last_sync_ms = 0U;
    bool autonomous = false;
    // One-shot guard: the latched CAN-TX fault (Fault::CAN_BUS) is reported
    // exactly once per boot.
    bool can_fault_reported = false;
};
