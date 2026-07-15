#pragma once

#include <cstddef>
#include <cstdint>

/// CRC-16/CCITT-FALSE via the STM32L4 CRC peripheral (ADR-011).
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
