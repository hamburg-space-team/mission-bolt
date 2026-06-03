#pragma once

#include "can_transport.hpp"
#include "node_computer.hpp"
#include "packet_types.hpp"

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

    // Packet builders - EXP3 overrides send_env_packet() for PayloadExp3Env
    virtual void send_env_packet(uint16_t can_tick, uint32_t timestamp_us);
    virtual void send_status_packet(uint16_t can_tick, uint32_t timestamp_us);

    virtual void on_experiment_init() noexcept {
    }
    virtual void on_experiment_tick(uint16_t can_tick, uint32_t timestamp_us) = 0;

    void send_gap(uint16_t first_tick, uint8_t count, PacketProtocol::GapReason reason, uint32_t timestamp_us);
    void on_sensor_failed() override;

    CanTransport& can; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    static constexpr uint8_t STATUS_INTERVAL = 25U;

  private:
    void on_init() override;
    void on_tick(uint32_t tick_start_us, uint16_t missed_periods) final;
    bool poll_sync(uint16_t& tick_out) noexcept;

    volatile bool sync_pending = false;
    volatile uint16_t sync_tick = 0U;
    static constexpr uint16_t NO_LAST_TICK = 0xFFFFU;
    uint16_t last_can_tick = NO_LAST_TICK;
    bool gc_done = false;
};
