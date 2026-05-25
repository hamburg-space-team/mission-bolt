#include "exp2_computer.hpp"
#include "can_protocol.hpp"
#include "main.h"

extern CAN_HandleTypeDef hcan1;

static Exp2Computer* instance_g = nullptr;

// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* hcan) {
    CAN_RxHeaderTypeDef hdr{};
    uint8_t data[CanProtocol::SYNC_DLC]{};
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &hdr, data) != HAL_OK) {
        return;
    }
    if (hdr.StdId == CanProtocol::SYNC_ID && instance_g != nullptr) {
        const auto tick = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8U);
        instance_g->notify_sync(tick);
    }
}

Exp2Computer::Exp2Computer(const Platform& platform, CmsisI2CBus& i2c, CanTransport& can) noexcept
    : ExpComputer(platform, i2c, can) {
    instance_g = this;
}

void Exp2Computer::on_experiment_tick(uint16_t /*can_tick*/, uint32_t /*timestamp_us*/) {
    // TODO: Do the exp here :)
}
