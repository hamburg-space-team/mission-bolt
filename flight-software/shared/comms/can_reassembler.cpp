#include "can_reassembler.hpp"
#include "packet_header.hpp"

#include <atomic>
#include <cstring>

CanReassembler::CanReassembler(std::span<const uint32_t> sources) noexcept : sources(sources) {
}

uint8_t CanReassembler::source_index(uint32_t can_id) const noexcept {
    for (uint8_t i = 0U; i < sources.size() && i < MAX_SOURCES; i++) {
        if (sources[i] == can_id) {
            return i;
        }
    }
    return MAX_SOURCES;
}

void CanReassembler::on_frame(uint32_t can_id, const uint8_t* raw) noexcept {
    const uint8_t idx = source_index(can_id);
    if (idx >= MAX_SOURCES) {
        return;
    }

    const auto frame_idx = static_cast<uint8_t>(raw[0] >> 4U);
    const auto frame_count = static_cast<uint8_t>(raw[0] & 0x0FU);

    Slot& slot = slots[idx];

    // Reset slot on a new sequence (frame 0). Discard mid-sequence if
    // we missed the previous frame
    if (frame_idx == 0U) {
        slot = {};
        slot.frames_expected = frame_count;
    } else if (frame_idx != slot.frames_received) {
        slot = {};
        return;
    }

    const auto offset = static_cast<uint8_t>(frame_idx * CanProtocol::BXCAN_BYTES_PER_FRAME);
    if (offset < CanProtocol::EXP_DATA_LEN) {
        const auto space = static_cast<uint8_t>(CanProtocol::EXP_DATA_LEN - offset);
        const auto chunk = (space < CanProtocol::BXCAN_BYTES_PER_FRAME) ? space : CanProtocol::BXCAN_BYTES_PER_FRAME;
        std::memcpy(slot.buf.data() + offset, raw + 1U, chunk);
    }
    slot.frames_received++;

    if (slot.frames_received != slot.frames_expected) {
        return;
    }

    // Packet complete - push into the consumer ring.
    const auto next = static_cast<uint8_t>((head + 1U) % RING_CAPACITY);
    if (next != tail) {
        ring[head] = slot.buf;

        std::atomic_signal_fence(std::memory_order_release);
        head = next;
    }
    // Either way the slot is reset for the next sequence on this source.
    slot = {};
}

bool CanReassembler::pop(uint8_t* out, uint8_t& len_out) noexcept {
    const auto current_tail = tail;
    if (current_tail == head) {
        return false;
    }
    // Acquire fence pairs with the producer's release above.
    std::atomic_signal_fence(std::memory_order_acquire);

    const auto& slot = ring[current_tail];
    const uint8_t payload_len = slot[PacketProtocol::HEADER_LENGTH_OFFSET];
    len_out = static_cast<uint8_t>(static_cast<uint8_t>(PacketProtocol::HEADER_SIZE) + payload_len +
                                   static_cast<uint8_t>(PacketProtocol::CRC_SIZE));
    std::memcpy(out, slot.data(), len_out);

    tail = static_cast<uint8_t>((current_tail + 1U) % RING_CAPACITY);
    return true;
}
