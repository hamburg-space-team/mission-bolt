#pragma once

#include "cmsis_i2c_bus.hpp"
#include "device_base.hpp"

#include <array>
#include <cstdint>

constexpr uint8_t MS5611_ADDR = 0x77U;

/// Oversampling ratio selector. Higher = better resolution, longer
/// conversion. Wait times in CONV_TIME_MS (datasheet).
///
/// @ingroup sensors
enum class MS5611Osr : uint8_t {
    OSR_256 = 0x00U,
    OSR_512 = 0x01U,
    OSR_1024 = 0x02U,
    OSR_2048 = 0x03U,
    OSR_4096 = 0x04U,
};

/// One raw barometer sample (uncompensated ADC). Compensation runs on
/// ground (ADR-009).
///
/// @ingroup sensors
struct MS5611Result {
    uint32_t d1; // pressure
    uint32_t d2; // temperature
};

/// TE Connectivity MS5611-01BA pressure/temperature sensor. Reads PROM
/// and verifies the CRC during init().
///
/// @ingroup sensors
class MS5611 : public DeviceBase {
  public:
    using delay_fn = void (*)(uint32_t ms);

    /// Reset, read PROM, verify CRC. delay=nullptr disables the
    /// conversion-time wait (caller polls instead). Returns 0 on
    /// success, -1 on bus error or PROM-CRC mismatch.
    [[nodiscard]] int init(CmsisI2CBus* bus, uint8_t addr = MS5611_ADDR, MS5611Osr osr = MS5611Osr::OSR_4096,
                           delay_fn delay = nullptr);

    /// Trigger D2 (temp) and D1 (pressure) conversions, return both
    /// raw ADC values. Failures latch via DeviceBase.
    [[nodiscard]] int read(MS5611Result* result);

  private:
    static constexpr uint8_t COEFF_COUNT = 8U;

    static constexpr uint8_t CMD_RESET = 0x1EU;
    static constexpr uint8_t CMD_ADC_READ = 0x00U;

    static constexpr uint8_t CMD_CONVERT_D1 = 0x40U;
    static constexpr uint8_t CMD_CONVERT_D2 = 0x50U;

    static constexpr uint8_t PROM_BASE_ADDR = 0xA0U;

    static constexpr uint8_t IDX_CRC = 7U;
    static constexpr uint8_t CRC_MASK = 0x0FU;

    static constexpr uint16_t CRC_POLY = 0x3000U;

    // Worst-case conversion time per OSR setting, ms.
    static constexpr std::array<uint16_t, 5> CONV_TIME_MS = {1U, 2U, 3U, 5U, 10U};

    CmsisI2CBus* bus = nullptr;
    uint8_t addr = 0U;
    MS5611Osr osr = MS5611Osr::OSR_4096;
    delay_fn delay_ms = nullptr;
    std::array<uint16_t, COEFF_COUNT> coeff = {};
    bool coeff_read = false;

    [[nodiscard]] int reset();
    [[nodiscard]] int read_prom();
    [[nodiscard]] int read_adc(uint8_t cmd, uint32_t* value);
    [[nodiscard]] int read_sample(MS5611Result* result);
    [[nodiscard]] bool prom_crc_ok() const;
};
