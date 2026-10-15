#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

/// Test patterns for the LiFi bit-error measurement (ICD-003 test
/// matrix). Generated at compile time; the same generator on the ground
/// reproduces the expected block from the pattern code alone.
///
/// @ingroup lifi
namespace LifiPattern {

    /// Pattern codes as ICD-003 lists them
    enum class Kind : uint8_t {
        PRBS7 = 0x00,       ///< x^7 + x^6 + 1, period 127 bits
        PRBS15 = 0x01,      ///< x^15 + x^14 + 1, period 32767 bits
        ALL_ONES = 0x02,    ///< every byte 0xFF
        ALTERNATING = 0x03, ///< every byte 0xAA
    };

    /// Fibonacci LFSR with taps at the two most significant stages, seeded
    /// all-ones; one output bit per shift, packed MSB first
    struct Lfsr {
        uint16_t state;
        uint8_t order;

        [[nodiscard]] constexpr uint8_t next_bit() noexcept {
            const auto tap_a = static_cast<uint8_t>((this->state >> (this->order - 1U)) & 1U);
            const auto tap_b = static_cast<uint8_t>((this->state >> (this->order - 2U)) & 1U);
            const auto bit = static_cast<uint8_t>(tap_a ^ tap_b);
            const auto mask = static_cast<uint16_t>((1U << this->order) - 1U);
            this->state = static_cast<uint16_t>(((this->state << 1U) | bit) & mask);
            return bit;
        }

        [[nodiscard]] constexpr uint8_t next_byte() noexcept {
            uint8_t byte = 0U;
            for (uint8_t i = 0U; i < 8U; i++) {
                byte = static_cast<uint8_t>((byte << 1U) | next_bit());
            }
            return byte;
        }
    };

    /// The 15-bit start state a raw seed selects. 0 would lock the LFSR at
    /// zero for good, so it maps to 0x7FFF, the Kind::PRBS15 default
    [[nodiscard]] constexpr uint16_t prbs15_seed(uint16_t raw) noexcept {
        const auto s = static_cast<uint16_t>(raw & 0x7FFFU);
        return (s == 0U) ? static_cast<uint16_t>(0x7FFFU) : s;
    }

    /// PRBS-15 starting from `seed` (see prbs15_seed). Each seed is a
    /// different phase of the 32767-bit sequence, so the ground rebuilds a
    /// round's block from the seed alone
    constexpr void fill_prbs15(std::span<uint8_t> out, uint16_t seed) noexcept {
        Lfsr lfsr{prbs15_seed(seed), 15U};
        for (uint8_t& b : out) {
            b = lfsr.next_byte();
        }
    }

    /// Write the first out.size() bytes of `kind` into `out`
    constexpr void fill(std::span<uint8_t> out, Kind kind) noexcept {
        switch (kind) {
        case Kind::PRBS7: {
            Lfsr lfsr{0x7FU, 7U};
            for (uint8_t& b : out) {
                b = lfsr.next_byte();
            }
            break;
        }
        case Kind::PRBS15:
            fill_prbs15(out, 0x7FFFU);
            break;
        case Kind::ALL_ONES:
            for (uint8_t& b : out) {
                b = 0xFFU;
            }
            break;
        case Kind::ALTERNATING:
            for (uint8_t& b : out) {
                b = 0xAAU;
            }
            break;
        }
    }

    /// Compile-time block of N bytes
    template <std::size_t N> [[nodiscard]] constexpr std::array<uint8_t, N> make(Kind kind) noexcept {
        std::array<uint8_t, N> block{};
        fill(std::span<uint8_t>(block), kind);
        return block;
    }

} // namespace LifiPattern
