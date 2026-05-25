#pragma once

#include "Driver_USART.h"
#include "icm42686.hpp"
#include "node_computer.hpp"
#include "packet_types.hpp"

#include <array>
#include <cstdint>

class BtcComputer final : public NodeComputer {
  public:
    explicit BtcComputer(const Platform& platform, CmsisI2CBus& i2c, ARM_DRIVER_USART& usart) noexcept;

  protected:
    void on_init() override;
    void on_tick(uint32_t tick_start_us, uint16_t missed_periods) override;
    void on_sensor_failed() override;
    void init_extra_sensors() override;

  private:
    static constexpr uint8_t SAVE_INTERVAL = 25U;
    static constexpr uint32_t DOWNLINK_BAUD = 115200U;

    ARM_DRIVER_USART& usart; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    uint16_t sync_count = 0U;
    bool lo_received = false;
    bool gc_done = false;
    uint32_t lo_rtc_s = 0U;

    void usart_wait();
    void send_gap_to_uart(uint16_t first_tick, uint8_t count, PacketProtocol::GapReason reason, uint32_t timestamp_us);
    void drain_exp_frames();
    void send_env_packet(uint32_t tick_start_us);
    void send_status_packet(uint32_t tick_start_us);

    ICM42686 imu{};
};
