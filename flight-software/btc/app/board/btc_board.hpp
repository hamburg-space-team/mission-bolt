#pragma once

#include "boot_state.hpp"

#include <cstdint>
#include <span>

/// Board I/O seam for the BTC
///
/// @ingroup apps
class BtcBoard {
  public:
    virtual ~BtcBoard() = default;
    BtcBoard(const BtcBoard&) = delete;
    BtcBoard& operator=(const BtcBoard&) = delete;
    BtcBoard(BtcBoard&&) = delete;
    BtcBoard& operator=(BtcBoard&&) = delete;

    /// Broadcast one 25 Hz SYNC frame: bus-wide tick, mission mode and
    /// self-test target (NodeId or CanProtocol::SELF_TEST_NONE). SYNC is the
    /// only frame the EXPs can receive
    virtual void send_sync(uint16_t tick, BootState::Mode mode, uint8_t test_target) noexcept = 0;

    /// Broadcast the STORAGE_START frame
    virtual void broadcast_storage_start() noexcept = 0;

    /// Bring the CAN peripheral up
    virtual void start_can(std::span<const uint32_t> rx_ids) noexcept = 0;

    [[nodiscard]] virtual bool lo_asserted() const noexcept = 0;
    [[nodiscard]] virtual bool soe_asserted() const noexcept = 0;
    [[nodiscard]] virtual bool sods_asserted() const noexcept = 0;

    /// Wall-clock seconds since midnight from the RTC
    [[nodiscard]] virtual uint32_t rtc_now_s() const noexcept = 0;

  protected:
    BtcBoard() = default;
};
