#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

/// @defgroup lifi LiFi transceiver

/// Host-UART command protocol of the STM32U031 LiFi transceiver, byte for
/// byte as ICD-008 specifies it: escape byte, one command byte, fixed
/// big-endian arguments, ACK/NAK replies. Pure constants and encoders, no
/// I/O.
///
/// @ingroup lifi
namespace LifiProtocol {

    inline constexpr uint8_t ESC = 0x5CU; ///< '\' prefixes every command
    inline constexpr uint8_t ACK = 0x06U;
    inline constexpr uint8_t NAK = 0x15U;

    /// Command bytes (ICD-008 command reference)
    enum class Cmd : uint8_t {
        SET_BUFFER = 0x4EU,       ///< 'N' + u16: TX buffer size, clears both buffers
        LOAD_BUFFER = 0x73U,      ///< 's' + exactly N raw bytes, no reply
        SEND_BUFFER = 0x53U,      ///< 'S': start the optical transmission
        SET_OPTICAL_BAUD = 0x66U, ///< 'f' + u32: optical link baud, both directions
        RX_ENABLE = 0x52U,        ///< 'R': front end on, capture
        RX_DISABLE = 0x58U,       ///< 'X': stop capture, front end off
        FEED_THROUGH = 0x46U,     ///< 'F': forward received bytes to the host as they arrive
        POLLING = 0x50U,          ///< 'P': buffer mode (default)
        CLEAR_RX = 0x43U,         ///< 'C': discard unread received bytes
        RX_COUNT = 0x3FU,         ///< '?' -> u16 little-endian
        READ_RX = 0x72U,          ///< 'r' -> raw bytes, no framing
        SET_THRESHOLD = 0x54U,    ///< 'T' + u16: comparator threshold 0..4095
        READ_PHOTODIODE = 0x64U,  ///< 'd' -> u16 big-endian ADC sample
    };

    /// Host UART rate, fixed in the transceiver firmware
    inline constexpr uint32_t HOST_BAUD = 115200U;
    /// Firmware accepts 240..1 000 000, the optical front end follows up
    /// to 460 800 (ICD-008 physical layer). Above that the ACK is a lie
    inline constexpr uint32_t OPTICAL_BAUD_MIN = 240U;
    inline constexpr uint32_t OPTICAL_BAUD_MAX = 460800U;
    /// Shared TX/RX region in the transceiver
    inline constexpr uint16_t BUFFER_TOTAL = 4000U;
    /// 's' truncates its length to 8 bits (ICD-008 limitation #1)
    inline constexpr uint16_t MAX_BLOCK_LEN = 255U;
    inline constexpr uint16_t THRESHOLD_MAX = 4095U;
    /// Reply deadline for any ACK/NAK once the command has reached the
    /// transceiver (ICD-008 timing)
    inline constexpr uint32_t ACK_TIMEOUT_MS = 20U;
    /// "Hello World\n\r", ~10 ms after transceiver reset. Flushed, never parsed
    inline constexpr uint8_t BOOT_BANNER_LEN = 13U;

    /// Wire time of `bytes` 8N1 frames at `baud`, rounded up to whole ms
    [[nodiscard]] constexpr uint32_t frame_time_ms(uint32_t bytes, uint32_t baud) noexcept {
        return ((bytes * 10U * 1000U) + baud - 1U) / baud;
    }

    /// Fixed-capacity command script: commands appended back to back, sent
    /// in one write. Overflow is sticky and checked by the driver before
    /// the script goes out, so a script that does not fit never goes out
    /// half-way (which would leave the transceiver in an unknown state).
    ///
    /// @ingroup lifi
    template <std::size_t Capacity> class Script {
      public:
        constexpr Script& cmd(Cmd c) noexcept {
            push(ESC);
            push(static_cast<uint8_t>(c));
            return *this;
        }

        constexpr Script& u16(Cmd c, uint16_t arg) noexcept {
            cmd(c);
            push(static_cast<uint8_t>(arg >> 8U));
            push(static_cast<uint8_t>(arg));
            return *this;
        }

        constexpr Script& u32(Cmd c, uint32_t arg) noexcept {
            cmd(c);
            push(static_cast<uint8_t>(arg >> 24U));
            push(static_cast<uint8_t>(arg >> 16U));
            push(static_cast<uint8_t>(arg >> 8U));
            push(static_cast<uint8_t>(arg));
            return *this;
        }

        /// Argument bytes are never escaped (ICD-008 framing rules)
        constexpr Script& raw(std::span<const uint8_t> bytes) noexcept {
            for (const uint8_t b : bytes) {
                push(b);
            }
            return *this;
        }

        [[nodiscard]] constexpr std::span<const uint8_t> bytes() const noexcept {
            return std::span<const uint8_t>(this->buf.data(), this->len);
        }
        [[nodiscard]] constexpr bool overflowed() const noexcept {
            return this->overflow;
        }
        constexpr void clear() noexcept {
            this->len = 0U;
            this->overflow = false;
        }

      private:
        constexpr void push(uint8_t b) noexcept {
            if (this->len >= Capacity) {
                this->overflow = true;
                return;
            }
            this->buf[this->len] = b;
            this->len++;
        }

        std::array<uint8_t, Capacity> buf{};
        std::size_t len = 0U;
        bool overflow = false;
    };

} // namespace LifiProtocol
