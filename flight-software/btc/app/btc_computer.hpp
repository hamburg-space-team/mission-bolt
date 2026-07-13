#pragma once

#include "Driver_USART.h"
#include "can_reassembler.hpp"
#include "icm42686.hpp"
#include "node_computer.hpp"
#include <bolt/wire/types.hpp>
#include "rs422_downlink.hpp"
#include "sync_broadcaster.hpp"
#include "uplink_parser.hpp"

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
    explicit BtcComputer(const Platform& platform, CmsisI2CBus& i2c, Store& storage, ARM_DRIVER_USART& usart) noexcept;

    /// Forwards the raw bxCAN frame into the reassembler.
    void notify_can_frame(uint32_t can_id, const uint8_t* raw) noexcept;

  protected:
    void on_init() override;
    void on_tick(uint32_t tick_start_us, uint16_t missed_periods) override;
    void on_drain(uint32_t deadline_ms) override;
    void report_fault(StatusLeds::Fault code, const Error& err) override;
    void init_extra_sensors() override;

  private:
    static constexpr uint8_t SAVE_INTERVAL = 25U;
    static constexpr uint8_t LO_LEVEL_DEBOUNCE_TICKS = 3U;

    static constexpr uint8_t IMU_RETRY_TICKS = 25U;

    static constexpr uint32_t DOWNLINK_BAUD = 38400U;

    Rs422Downlink downlink;
    CanReassembler reassembler;
    SyncBroadcaster sync_tx;
    PacketProtocol::UplinkParser uplink;
    uint16_t sync_count = 0U;
    bool lo_received = false;
    uint8_t lo_level_ticks = 0U;
    uint8_t imu_retry_ticks = 0U;
    bool gc_done = false;

    bool uart_fault_reported = false;
    bool imu_fault_reported = false;
    uint32_t lo_rtc_s = 0U;

    void send_gap_to_uart(uint16_t first_tick, uint8_t count, PacketProtocol::GapReason reason, uint32_t timestamp_us);
    /// Drain any received uplink bytes, frame them, and acknowledge each
    /// complete command. The command handlers themselves are intentionally a
    /// no-op for now (hook in handle_uplink); the ACK confirms receipt.
    void poll_uplink(uint32_t timestamp_us);
    void handle_uplink(uint8_t opcode, uint8_t seq, uint32_t timestamp_us);
    void drain_exp_frames();
    void send_env_packet(uint32_t tick_start_us);
    void send_status_packet(uint32_t tick_start_us);
    void send_imu_packet(uint32_t timestamp_us);
    // Downlink BTC's worst-case scope timings (RS-422), ~1 Hz.
    void send_timing(uint32_t timestamp_us);

    ICM42686 imu{};
};
