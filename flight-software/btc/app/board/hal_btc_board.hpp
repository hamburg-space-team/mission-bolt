#pragma once

#include "btc_board.hpp"
#include "main.h" // CAN_HandleTypeDef, RTC_HandleTypeDef, HAL_*

#include <cstdint>
#include <span>

/// Flight implementation of BtcBoard: owns the CAN and RTC handles and issues
/// the STM32 HAL calls
///
/// @ingroup apps
class HalBtcBoard final : public BtcBoard {
  public:
    HalBtcBoard(CAN_HandleTypeDef& hcan, RTC_HandleTypeDef& hrtc) noexcept;

    void send_sync(uint16_t tick) noexcept override;
    void broadcast_storage_start() noexcept override;
    void start_can(std::span<const uint32_t> rx_ids) noexcept override;

    [[nodiscard]] bool lo_asserted() const noexcept override;
    [[nodiscard]] bool soe_asserted() const noexcept override;
    [[nodiscard]] bool sods_asserted() const noexcept override;

    [[nodiscard]] uint32_t rtc_now_s() const noexcept override;

  private:
    CAN_HandleTypeDef& hcan; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    RTC_HandleTypeDef& hrtc; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};
