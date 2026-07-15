#include "btc_isr.hpp"
#include "btc/btc_computer.hpp"
#include "main.h"
#include "timing.hpp"

#include <array>

namespace {

    // The bound computer
    BtcComputer* instance_g = nullptr;

    // PB11 = IMU_INT1
    constexpr uint16_t IMU_INT1_PIN = GPIO_PIN_11;

} // namespace

void BtcIsr::bind(BtcComputer& computer) noexcept {
    instance_g = &computer;
}

extern "C" void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef* hcan) {
    CAN_RxHeaderTypeDef hdr{};
    std::array<uint8_t, 8U> raw{};
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO1, &hdr, raw.data()) != HAL_OK) {
        return;
    }
    if (instance_g != nullptr) {
        instance_g->notify_can_frame(hdr.StdId, raw.data());
    }
}

// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (instance_g == nullptr) {
        return;
    }
    if (GPIO_Pin == LO_Pin) {
        instance_g->notify_lo_edge();
    }
    if (GPIO_Pin == IMU_INT1_PIN) {
        instance_g->notify_imu_drdy(Timing::us_now());
    }
}
