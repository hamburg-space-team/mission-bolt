#include "crc16_hw.hpp"
#include "crc16.hpp"
#include "main.h" // IWYU pragma: keep

#include <array>

extern CRC_HandleTypeDef hcrc;

namespace {
    // Set by init(): true = peripheral reconfigured and self-test passed.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    bool hw_verified = false;

    uint16_t compute_hw(const uint8_t* data, std::size_t len) noexcept {
        __HAL_CRC_DR_RESET(&hcrc); // reload INIT (0xFFFF)

        // Byte-wide writes to DR make the peripheral consume one byte per write
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        volatile auto* dr8 = reinterpret_cast<volatile uint8_t*>(&hcrc.Instance->DR);
        for (std::size_t i = 0U; i < len; i++) {
            *dr8 = data[i];
        }

        return static_cast<uint16_t>(hcrc.Instance->DR);
    }
} // namespace

void Crc16Hw::init() noexcept {
    // MX_CRC_Init() leaves the peripheral at the CubeMX CRC-32 defaults;
    // reprogram it to the packet CRC-16/CCITT-FALSE (ICD-006).
    hcrc.Instance = CRC;
    hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_DISABLE;
    hcrc.Init.GeneratingPolynomial = 0x1021U;
    hcrc.Init.CRCLength = CRC_POLYLENGTH_16B;
    hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_DISABLE;
    hcrc.Init.InitValue = 0xFFFFU;
    hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
    hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
    hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
    if (HAL_CRC_Init(&hcrc) != HAL_OK) {
        hw_verified = false;
        return;
    }

    // Standard check value of CRC-16/CCITT-FALSE
    static constexpr std::array<uint8_t, 9> check = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    hw_verified = (compute_hw(check.data(), check.size()) == 0x29B1U);
}

uint16_t Crc16Hw::compute(const uint8_t* data, std::size_t len) noexcept {
    if (!hw_verified) {
        return Crc::compute(data, len); // proven constexpr software path
    }
    return compute_hw(data, len);
}
