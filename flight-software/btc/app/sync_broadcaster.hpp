#pragma once

#include "can_protocol.hpp"
#include "main.h" // IWYU pragma: keep

#include <array>
#include <cstdint>

/// BTC -> EXP outbound control bus
///
/// @ingroup apps
class SyncBroadcaster {
  public:
    explicit SyncBroadcaster(CAN_HandleTypeDef& hcan) noexcept : hcan(hcan) {
    }

    SyncBroadcaster(const SyncBroadcaster&) = delete;
    SyncBroadcaster& operator=(const SyncBroadcaster&) = delete;
    SyncBroadcaster(SyncBroadcaster&&) = delete;
    SyncBroadcaster& operator=(SyncBroadcaster&&) = delete;

    /// Push a SYNC frame into the next free TX mailbox. No-op if all
    /// three are full (HAL_CAN_AddTxMessage returns HAL_ERROR, which
    /// we deliberately ignore - the next tick will push a fresh frame
    /// with the next tick value, so a single skipped SYNC is just one
    /// dropped beat in the EXPs' SYNC observation).
    void send(uint16_t tick) noexcept {
        CAN_TxHeaderTypeDef hdr{};
        hdr.StdId = CanProtocol::SYNC_ID;
        // hdr.IDE = 0 (CAN_ID_STD), hdr.RTR = 0 (CAN_RTR_DATA): zero-init.
        hdr.DLC = CanProtocol::SYNC_DLC;
        hdr.TransmitGlobalTime = DISABLE;

        const std::array<uint8_t, CanProtocol::SYNC_DLC> data = {
            static_cast<uint8_t>(tick),
            static_cast<uint8_t>(tick >> 8U),
        };
        uint32_t mailbox = 0U;
        HAL_CAN_AddTxMessage(&hcan, &hdr, data.data(), &mailbox);
    }

    /// Push a STORAGE_START frame
    void broadcast_storage_start() noexcept {
        CAN_TxHeaderTypeDef hdr{};
        hdr.StdId = CanProtocol::STORAGE_START_ID;
        hdr.DLC = CanProtocol::STORAGE_START_DLC;
        hdr.TransmitGlobalTime = DISABLE;

        uint32_t mailbox = 0U;
        // DLC=0 frame: HAL_CAN_AddTxMessage requires a non-null data
        // pointer even when DLC is 0, so feed it the address of a
        // local zero. The CAN controller will not transmit any data
        // bytes (DLC governs that), only the ID + frame overhead.
        const uint8_t placeholder = 0U;
        HAL_CAN_AddTxMessage(&hcan, &hdr, &placeholder, &mailbox);
    }

  private:
    CAN_HandleTypeDef& hcan; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};
