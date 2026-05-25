#pragma once

#include "packet_header.hpp"
#include "packet_payloads.hpp"
#include "packet_types.hpp"

#include <array>
#include <cstdint>
#include <cstring>

#include "crc16.hpp"

static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "little-endian required");

namespace PacketProtocol {

    struct Tick {
        uint16_t value;
        explicit Tick(uint16_t v) noexcept : value(v) {
        }
    };

    struct TimestampUs {
        uint32_t value;
        explicit TimestampUs(uint32_t v) noexcept : value(v) {
        }
    };

    class PacketBuilder {
      private:
        std::array<uint8_t, 256> seq = {};

      public:
        void init() noexcept {
            seq.fill(0U);
        }

        [[nodiscard]] uint8_t build(uint8_t* buf, PayloadType type, Tick tick, TimestampUs ts_us, const void* payload,
                                    uint8_t payload_len) noexcept {
            const uint16_t total = static_cast<uint16_t>(HEADER_SIZE) + payload_len + CRC_SIZE;

            if (payload_len > MAX_PAYLOAD || total > MAX_PACKET_SIZE) {
                return 0U;
            }

            uint8_t offset = 0U;

            buf[offset++] = SYNC_0;
            buf[offset++] = SYNC_1;
            buf[offset++] = PROTOCOL_VERSION;
            buf[offset++] = static_cast<uint8_t>(type);

            const auto type_idx = static_cast<uint8_t>(type);
            buf[offset++] = seq[type_idx]++;

            buf[offset++] = payload_len;

            buf[offset++] = static_cast<uint8_t>(tick.value);
            buf[offset++] = static_cast<uint8_t>(tick.value >> 8U);

            buf[offset++] = static_cast<uint8_t>(ts_us.value);
            buf[offset++] = static_cast<uint8_t>(ts_us.value >> 8U);
            buf[offset++] = static_cast<uint8_t>(ts_us.value >> 16U);
            buf[offset++] = static_cast<uint8_t>(ts_us.value >> 24U);

            std::memcpy(buf + offset, payload, payload_len);

            // CRC over everything after the 2 sync bytes
            const auto crc_len = static_cast<std::size_t>(HEADER_SIZE - 2U + payload_len);
            const uint16_t crc = Crc::compute(buf + 2U, crc_len);

            const auto crc_offset = static_cast<uint16_t>(HEADER_SIZE + payload_len);
            buf[crc_offset] = static_cast<uint8_t>(crc >> 8U);
            buf[crc_offset + 1U] = static_cast<uint8_t>(crc);

            return static_cast<uint8_t>(total);
        }

        [[nodiscard]] uint8_t build_gap(uint8_t* buf, Tick tick, TimestampUs ts_us, PayloadGapMarker gap) noexcept {
            return build(buf, PayloadType::GAP_MARKER, tick, ts_us, &gap, sizeof(gap));
        }

        [[nodiscard]] uint8_t build_boot(uint8_t* buf, BootReason reason, uint16_t reboot_count) noexcept {
            PayloadBoot boot{};
            boot.reason = reason;
            boot.reboot_count = reboot_count;
            return build(buf, PayloadType::BOOT, Tick{0U}, TimestampUs{0U}, &boot, sizeof(boot));
        }
    };

} // namespace PacketProtocol
