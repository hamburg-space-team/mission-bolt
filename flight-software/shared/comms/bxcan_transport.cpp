#include "bxcan_transport.hpp"
#include "can_protocol.hpp"
#include "main.h"

#include <array>
#include <cstring>

extern CAN_HandleTypeDef hcan1;

void BxcanTransport::send(uint32_t id, const uint8_t* data, uint8_t len) {
    const auto frame_count =
        static_cast<uint8_t>((len + CanProtocol::BXCAN_BYTES_PER_FRAME - 1U) / CanProtocol::BXCAN_BYTES_PER_FRAME);

    CAN_TxHeaderTypeDef hdr{};
    hdr.StdId = id;
    // hdr.IDE = 0 (CAN_ID_STD)   - standard 11-bit frame, set by zero-init
    // hdr.RTR = 0 (CAN_RTR_DATA) - data frame, set by zero-init
    hdr.DLC = 8U;
    hdr.TransmitGlobalTime = DISABLE;

    for (uint8_t i = 0U; i < frame_count; ++i) {
        std::array<uint8_t, 8U> frame{};
        frame[0] = static_cast<uint8_t>((i << 4U) | (frame_count & 0x0FU));

        const auto offset = static_cast<uint8_t>(i * CanProtocol::BXCAN_BYTES_PER_FRAME);
        const auto remaining = static_cast<uint8_t>(len - offset);
        const uint8_t chunk =
            (remaining < CanProtocol::BXCAN_BYTES_PER_FRAME) ? remaining : CanProtocol::BXCAN_BYTES_PER_FRAME;
        std::memcpy(&frame[1], data + offset, chunk);

        uint32_t tickstart = HAL_GetTick(); // Capture start time

        // Wait until a mailbox is free or timeout expires
        while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0U) {
            if ((HAL_GetTick() - tickstart) >= 2U) {
                return;
            }
        }

        uint32_t mailbox = 0U;
        HAL_CAN_AddTxMessage(&hcan1, &hdr, frame.data(), &mailbox);
    }
}
