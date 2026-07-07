#pragma once

#include "can_transport.hpp"

#include <array>
#include <cstdint>

/// bxCAN implementation for STM32L4xx targets (no FDCAN, max 8 bytes
/// per frame). Fragments data into 8-byte CAN frames:
///   byte[0] = (frame_index << 4) | frame_count - 4 bits each, max 15 frames
///   bytes[1..7] = payload chunk (zero-padded on last frame)
/// Receiver (BTC) reassembles using frame_count in byte[0].
///
///
/// @ingroup comms
class BxcanTransport final : public CanTransport {
  public:
    /// Registers this instance as the HAL TX-callback target (the HAL
    /// callbacks carry no user context). Exactly one transport per binary.
    BxcanTransport() noexcept;

    /// Enqueue one packet (fragmented into frames) - never blocks.
    void send(uint32_t id, const uint8_t* data, uint8_t len) override;

    /// Latched true once MAX_CONSECUTIVE_DROPS packets in a row were
    /// dropped - the ring never drains, so the bus is dead or permanently
    /// saturated. Polled once per tick; feeds StatusLeds::Fault::CAN_BUS.
    [[nodiscard]] bool is_failed() const override {
        return this->consecutive_drops >= MAX_CONSECUTIVE_DROPS;
    }

    /// Called by the HAL TX-mailbox-complete callbacks (interrupt
    /// context): refill the freed mailboxes from the ring.
    void on_tx_complete() noexcept;

  private:
    struct Frame {
        uint32_t id = 0U;
        std::array<uint8_t, 8U> bytes{};
    };
    // 64 frames = ~6 full 64-byte packets of headroom. Drained at wire
    // speed (~55-130 us per frame at 1 Mbps), so it only fills while the
    // bus is saturated or down.
    static constexpr uint16_t TX_RING_FRAMES = 64U;
    static constexpr uint8_t MAX_CONSECUTIVE_DROPS = 10U;

    [[nodiscard]] uint16_t free_frames() const noexcept;
    void kick() noexcept;

    std::array<Frame, TX_RING_FRAMES> ring{};
    volatile uint16_t head = 0U; // producer (tick loop)
    volatile uint16_t tail = 0U; // consumer (TX-complete interrupt)
    uint8_t consecutive_drops = 0U;
};
