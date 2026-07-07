#include "bxcan_transport.hpp"
#include "can_protocol.hpp"
#include "main.h" // IWYU pragma: keep

#include <cstring>

extern CAN_HandleTypeDef hcan1;

namespace {
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    BxcanTransport* instance_g = nullptr;
} // namespace

BxcanTransport::BxcanTransport() noexcept {
    instance_g = this;
}

void BxcanTransport::send(uint32_t id, const uint8_t* data, uint8_t len) {
    const auto frame_count =
        static_cast<uint8_t>((len + CanProtocol::BXCAN_BYTES_PER_FRAME - 1U) / CanProtocol::BXCAN_BYTES_PER_FRAME);

    if (free_frames() < frame_count) {
        // Whole packet dropped: a partial packet would poison reassembly.
        if (this->consecutive_drops < MAX_CONSECUTIVE_DROPS) {
            this->consecutive_drops++;
        }
        return;
    }
    this->consecutive_drops = 0U;

    uint16_t h = this->head;
    for (uint8_t i = 0U; i < frame_count; ++i) {
        Frame& frame = this->ring[h];
        frame.id = id;
        frame.bytes.fill(0U);
        frame.bytes[0] = static_cast<uint8_t>((i << 4U) | (frame_count & 0x0FU));

        const auto offset = static_cast<uint8_t>(i * CanProtocol::BXCAN_BYTES_PER_FRAME);
        const auto remaining = static_cast<uint8_t>(len - offset);
        const uint8_t chunk =
            (remaining < CanProtocol::BXCAN_BYTES_PER_FRAME) ? remaining : CanProtocol::BXCAN_BYTES_PER_FRAME;
        std::memcpy(&frame.bytes[1], data + offset, chunk);

        h = static_cast<uint16_t>((h + 1U) % TX_RING_FRAMES);
    }
    this->head = h; // publish only after every frame is in place
    kick();
}

uint16_t BxcanTransport::free_frames() const noexcept {
    const uint16_t h = this->head;
    const uint16_t t = this->tail;
    const auto used = static_cast<uint16_t>((h + TX_RING_FRAMES - t) % TX_RING_FRAMES);
    return static_cast<uint16_t>((TX_RING_FRAMES - 1U) - used);
}

void BxcanTransport::kick() noexcept {
    // Fill every free mailbox from the ring. Runs from both the producer
    // (send) and the TX-complete interrupt; the short IRQ lock keeps the
    // tail handoff atomic between them.
    __disable_irq();
    while (this->tail != this->head && HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0U) {
        Frame& frame = this->ring[this->tail];

        CAN_TxHeaderTypeDef hdr{};
        hdr.StdId = frame.id;
        // hdr.IDE = 0 (CAN_ID_STD), hdr.RTR = 0 (CAN_RTR_DATA): zero-init.
        hdr.DLC = 8U;
        hdr.TransmitGlobalTime = DISABLE;

        uint32_t mailbox = 0U;
        if (HAL_CAN_AddTxMessage(&hcan1, &hdr, frame.bytes.data(), &mailbox) != HAL_OK) {
            break; // mailbox raced away; the next TX-complete event retries
        }
        this->tail = static_cast<uint16_t>((this->tail + 1U) % TX_RING_FRAMES);
    }
    __enable_irq();
}

void BxcanTransport::on_tx_complete() noexcept {
    kick();
}

// HAL weak-callback overrides, one per mailbox - all funnel into the ring
// refill. Interrupt context; guarded for binaries without a transport (BTC).
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef* /*hcan*/) {
    if (instance_g != nullptr) {
        instance_g->on_tx_complete();
    }
}
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef* /*hcan*/) {
    if (instance_g != nullptr) {
        instance_g->on_tx_complete();
    }
}
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef* /*hcan*/) {
    if (instance_g != nullptr) {
        instance_g->on_tx_complete();
    }
}
