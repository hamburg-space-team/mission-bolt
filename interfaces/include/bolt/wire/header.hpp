#pragma once

#include <cstdint>

namespace PacketProtocol {

    constexpr uint8_t SYNC_0 = 0xB0U;
    constexpr uint8_t SYNC_1 = 0x17U;
    constexpr uint8_t PROTOCOL_VERSION = 0x01U;
    constexpr uint16_t MAX_PACKET_SIZE = 64U;
    constexpr uint16_t HEADER_SIZE = 12U;
    constexpr uint16_t CRC_SIZE = 2U;
    constexpr uint16_t MAX_PAYLOAD = MAX_PACKET_SIZE - HEADER_SIZE - CRC_SIZE;
    constexpr uint8_t SPECTRUM_CHANNELS = 18U;
    static constexpr uint8_t HEADER_LENGTH_OFFSET = 5U;

    /// Downlink packet header
    ///
    /// @ingroup comms
    struct __attribute__((packed)) PacketHeader {
        uint8_t sync[2];       // NOLINT(modernize-avoid-c-arrays) - SYNC_0, SYNC_1
        uint8_t version;       ///< PROTOCOL_VERSION
        uint8_t type;          ///< PayloadType
        uint8_t sequence;      ///< per-type counter, wraps at 255
        uint8_t length;        ///< payload length in bytes
        uint16_t tick;         ///< 25 Hz tick since LO
        uint32_t timestamp_us; ///< microseconds since LO
    };

    static_assert(sizeof(PacketHeader) == HEADER_SIZE, "PacketHeader size mismatch");

} // namespace PacketProtocol
