#pragma once

#include <cstddef>
#include <cstdint>

/// CRC-16/CCITT-FALSE via the STM32L4 CRC peripheral (ADR-011).
///
/// CubeMX's MX_CRC_Init() brings the peripheral up with its CRC-32
/// defaults; init() reprograms it to the packet CRC from ICD-006
/// (polynomial 0x1021, init 0xFFFF, no reflection) and then runs the
/// standard check value ("123456789" -> 0x29B1). If that self-test
/// fails, compute() transparently falls back to the constexpr software
/// reference in crc16.hpp - the result is identical either way, only
/// slower.
///
/// NOT ISR-safe: the CRC peripheral is a single shared instance and
/// compute() resets it. The only user is PacketBuilder, which runs
/// exclusively in the main loop.
///
/// This header is host-safe (no HAL includes); the implementation file
/// is target-only and listed in shared.clayer.yml.
///
/// @ingroup utils
namespace Crc16Hw {

    /// Reconfigure the CRC peripheral to CRC-16/CCITT-FALSE and run the
    /// self-test. Call once at boot - PacketBuilder::init() does.
    void init() noexcept;

    /// CRC over data[0..len-1]. Hardware when the boot self-test passed,
    /// software fallback otherwise.
    [[nodiscard]] uint16_t compute(const uint8_t* data, std::size_t len) noexcept;

} // namespace Crc16Hw
