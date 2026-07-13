#pragma once

#include <bolt/wire/header.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace PacketProtocol {

    /// Byte-at-a-time framer for uplink commands (RXSM TC -> BTC). Fed the
    /// receive stream one byte at a time; emits a frame once a full
    /// CRC-valid packet has arrived. Resyncs on the SYNC word, so line noise
    /// or a partial frame never wedges it.
    ///
    /// Self-contained on purpose: it carries its own software CRC-16/CCITT
    /// (poly 0x1021, init 0xFFFF, no reflection - identical to the downlink
    /// CRC) rather than the shared hardware CRC peripheral, so it is safe to
    /// run in the main loop alongside PacketBuilder without contending for
    /// that peripheral, and trivially unit-testable off-target.
    ///
    /// @ingroup apps
    class UplinkParser {
      public:
        struct Frame {
            uint8_t opcode; ///< CommandOpcode (header type byte)
            uint8_t seq;    ///< uplink sequence number
        };

        /// Feed one received byte. Returns true and fills `out` exactly when a
        /// complete, CRC- and version-valid frame has just been assembled.
        [[nodiscard]] bool push(uint8_t byte, Frame& out) noexcept {
            switch (state) {
                case State::SYNC0:
                    if (byte == SYNC_0) {
                        buf[0] = byte;
                        state = State::SYNC1;
                    }
                    return false;

                case State::SYNC1:
                    if (byte == SYNC_1) {
                        buf[1] = byte;
                        idx = 2U;
                        state = State::HEADER;
                    } else if (byte != SYNC_0) {
                        // Not B0 B0.. and not B0 17 -> drop back to hunting.
                        state = State::SYNC0;
                    }
                    return false;

                case State::HEADER:
                    buf[idx++] = byte;
                    if (idx == HEADER_SIZE) {
                        len = buf[5];
                        if (len > MAX_PAYLOAD) {
                            reset(); // corrupt length field - resync
                            return false;
                        }
                        need = static_cast<uint16_t>(HEADER_SIZE + len + CRC_SIZE);
                        state = State::BODY;
                    }
                    return false;

                case State::BODY:
                    buf[idx++] = byte;
                    if (idx >= need) {
                        const bool ok = verify(out);
                        reset();
                        return ok;
                    }
                    return false;
            }
            return false;
        }

      private:
        bool verify(Frame& out) const noexcept {
            if (buf[2] != PROTOCOL_VERSION) {
                return false;
            }
            // CRC covers everything after the 2 sync bytes through the payload,
            // matching PacketBuilder::build.
            const auto crc_len = static_cast<std::size_t>(HEADER_SIZE - 2U + len);
            const uint16_t calc = crc16(&buf[2], crc_len);
            const auto crc_at = static_cast<uint16_t>(HEADER_SIZE + len);
            const auto rx = static_cast<uint16_t>((static_cast<uint16_t>(buf[crc_at]) << 8U) | buf[crc_at + 1U]);
            if (calc != rx) {
                return false;
            }
            out.opcode = buf[3];
            out.seq = buf[4];
            return true;
        }

        static uint16_t crc16(const uint8_t* data, std::size_t len) noexcept {
            uint16_t crc = 0xFFFFU;
            for (std::size_t i = 0U; i < len; i++) {
                crc = static_cast<uint16_t>(crc ^ (static_cast<uint16_t>(data[i]) << 8U));
                for (uint8_t b = 0U; b < 8U; b++) {
                    crc = static_cast<uint16_t>((crc & 0x8000U) != 0U ? (crc << 1U) ^ 0x1021U : (crc << 1U));
                }
            }
            return crc;
        }

        void reset() noexcept {
            state = State::SYNC0;
            idx = 0U;
        }

        enum class State : uint8_t { SYNC0, SYNC1, HEADER, BODY };
        State state = State::SYNC0;
        std::array<uint8_t, MAX_PACKET_SIZE> buf{};
        uint16_t idx = 0U;
        uint16_t need = 0U;
        uint8_t len = 0U;
    };

} // namespace PacketProtocol
