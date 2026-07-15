#include "exp2_computer.hpp"
#include "can_protocol.hpp"
#include "main.h" // IWYU pragma: keep
#include <bolt/wire/payloads.hpp>

extern CAN_HandleTypeDef hcan1;

namespace {
    Exp2Computer* instance_g = nullptr;
}
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* hcan) {
    CAN_RxHeaderTypeDef hdr{};

    std::array<uint8_t, 8U> data{};
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &hdr, data.data()) != HAL_OK) {
        return;
    }
    if (hdr.StdId == CanProtocol::SYNC_ID && hdr.DLC >= CanProtocol::SYNC_DLC && instance_g != nullptr) {
        const auto tick = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8U);
        instance_g->notify_sync(tick);
    }
}

Exp2Computer::Exp2Computer(const Platform& platform, CmsisI2CBus& i2c, Store& storage, CanTransport& can) noexcept
    : ExpComputer(platform, i2c, storage, can) {
    instance_g = this;
}

void Exp2Computer::on_experiment_tick(uint16_t /*can_tick*/, uint32_t /*timestamp_us*/) {
    // TODO: Do the exp here :)
}

void Exp2Computer::send_status_packet(uint16_t can_tick, uint32_t timestamp_us) {
    using namespace PacketProtocol;
    PayloadExpStatus status{};
    status.uptime_s = platform.tick_ms() / 1000U;
    status.sd_status = static_cast<uint8_t>(storage.is_mounted() ? 0x01U : 0x00U);

    if (auto len = pkt.build(tx_buf.data(), exp_status_type(), Tick{can_tick}, TimestampUs{timestamp_us}, &status,
                             static_cast<uint8_t>(sizeof(status)))) {
        can.send(exp_can_id(), tx_buf.data(), *len);
        (void)storage.write(tx_buf.data(), *len);
    }
}
