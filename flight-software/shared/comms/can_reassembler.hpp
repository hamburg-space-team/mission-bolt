#pragma once

#include "can_protocol.hpp"

#include <array>
#include <cstdint>
#include <span>

/// Reassembles fragmented EXP CAN frames back into complete
/// PacketProtocol packets. One slot per tracked source (EXP1/2/3 on
/// the BTC); when all expected fragments have arrived, the assembled
/// packet is pushed into an internal ring buffer where the consumer
/// (BTC tick loop) drains it.
///
/// @ingroup comms
class CanReassembler {
  public:
    /// Maximum tracked CAN source IDs (BTC has 3: EXP1/2/3, +1 slack).
    static constexpr uint8_t MAX_SOURCES = 4U;

    /// Completed-packet ring depth. ISR drops the oldest if the main thread doesn't keep up.
    static constexpr uint8_t RING_CAPACITY = 8U;

    /// sources: list of CAN IDs to track. Frames whose StdId is not in
    /// this list are ignored silently.
    explicit CanReassembler(std::span<const uint32_t> sources) noexcept;

    /// Called from the HAL CAN RX FIFO ISR. raw must point to at
    /// least 8 bytes (the bxCAN payload)
    void on_frame(uint32_t can_id, const uint8_t* raw) noexcept;

    /// Main-thread consumer. Returns true and fills (out, len_out) if
    /// a complete packet is ready; returns false if the ring is empty.
    [[nodiscard]] bool pop(uint8_t* out, uint8_t& len_out) noexcept;

  private:
    struct Slot {
        std::array<uint8_t, CanProtocol::EXP_DATA_LEN> buf{};
        uint8_t frames_received = 0U;
        uint8_t frames_expected = 0U;
    };

    /// Returns the source index for can_id, or MAX_SOURCES if unknown.
    [[nodiscard]] uint8_t source_index(uint32_t can_id) const noexcept;

    std::span<const uint32_t> sources;
    std::array<Slot, MAX_SOURCES> slots{};
    std::array<std::array<uint8_t, CanProtocol::EXP_DATA_LEN>, RING_CAPACITY> ring{};

    // Producer (ISR) advances head; consumer (main thread) advances
    // tail. Drop-on-full: ISR drops the new packet if (next == tail).
    volatile uint8_t head = 0U;
    volatile uint8_t tail = 0U;
};
