#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

// CRC-16/CCITT-FALSE  polynomial 0x1021, init 0xFFFF, no reflection
namespace Crc {

    constexpr uint16_t POLY = 0x1021;
    constexpr uint16_t INIT = 0xFFFF;

    // Updates CRC with one byte, returns new CRC value.
    constexpr uint16_t update(uint16_t crc, uint8_t byte) noexcept {
        crc ^= static_cast<uint16_t>(byte) << 8;
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 0x8000) != 0 ? (crc << 1) ^ POLY : crc << 1;
        }
        return crc;
    }

    // Computes CRC over data[0..size-1], returns 16-bit CRC value.
    constexpr uint16_t compute(const uint8_t* data, std::size_t size) noexcept {
        uint16_t crc = INIT;
        for (std::size_t i = 0; i < size; ++i) {
            crc = update(crc, data[i]);
        }
        return crc;
    }

    // Appends 2-byte CRC big-endian to buf[len], returns total length (len + 2)
    // buf must have capacity for at least len + 2 bytes
    inline std::size_t append(uint8_t* buf, std::size_t len) noexcept {
        uint16_t crc = compute(buf, len);
        buf[len] = static_cast<uint8_t>(crc >> 8);
        buf[len + 1] = static_cast<uint8_t>(crc & 0xFF);
        return len + 2;
    }

    // Returns true if the last 2 bytes of buf are a valid CRC over the preceding bytes
    inline bool verify(const uint8_t* frame, std::size_t size) noexcept {
        if (size < 3) {
            return false;
        }

        uint16_t expected = compute(frame, size - 2);
        uint16_t received = static_cast<uint16_t>(frame[size - 2]) << 8 | frame[size - 1];
        return expected == received;
    }

} // namespace Crc
