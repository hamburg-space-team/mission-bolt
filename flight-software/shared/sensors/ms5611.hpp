#pragma once

#include "cmsis_i2c_bus.hpp"
#include "device_base.hpp"
#include "errors.hpp"

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
    /// conversion-time wait (caller polls instead).
    [[nodiscard]] Result<void> init(CmsisI2CBus* bus, uint8_t addr = MS5611_ADDR, MS5611Osr osr = MS5611Osr::OSR_4096,
                                    delay_fn delay = nullptr);

    /// Pipelined per-tick read (ICD-005): collects the conversion started on
    /// the PREVIOUS call, starts the next one (D2/D1 alternating) and returns
    /// the most recent completed pair - one value is one tick older than the
    /// other. Never blocks for a conversion. Requires calls to be at least
    /// one conversion time apart (40 ms tick >> 9.04 ms max at OSR_4096).
    /// Returns TIMEOUT (without a DeviceBase strike) for the first two calls
    /// after init while the pipeline primes. Failures latch via DeviceBase.
    [[nodiscard]] Result<MS5611Result> read();

    /// Re-read the PROM into a local copy and re-verify its factory CRC-4
    /// (self-test identity check - the chip has no WHO_AM_I). Diagnostic
    /// only, no failure latch. Returns C1 as a per-device fingerprint
    [[nodiscard]] Result<uint16_t> verify_prom();

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

    // Worst-case conversion time per OSR setting, ms (datasheet). Not waited
    // on at runtime: the pipelined read() relies on the 40 ms tick period
    // exceeding the worst case (10 ms at OSR_4096) instead of blocking.
    static constexpr std::array<uint16_t, 5> CONV_TIME_MS = {1U, 2U, 3U, 5U, 10U};

    /// Which conversion is currently running in the device (pipeline state).
    enum class Pending : uint8_t { NONE, D1, D2 };

    CmsisI2CBus* bus = nullptr;
    uint8_t addr = 0U;
    MS5611Osr osr = MS5611Osr::OSR_4096;
    delay_fn delay_ms = nullptr;
    std::array<uint16_t, COEFF_COUNT> coeff = {};
    bool coeff_read = false;

    // Pipeline state: in-flight conversion plus the most recent raw values.
    Pending pending = Pending::NONE;
    uint32_t d1_raw = 0U;
    uint32_t d2_raw = 0U;
    bool have_d1 = false;
    bool have_d2 = false;

    [[nodiscard]] Result<void> reset();
    [[nodiscard]] Result<void> read_prom();
    [[nodiscard]] Result<void> start_conversion(uint8_t cmd);
    [[nodiscard]] Result<uint32_t> read_adc_result();
    [[nodiscard]] Result<void> collect_pending();
    [[nodiscard]] static bool prom_crc_ok(const std::array<uint16_t, COEFF_COUNT>& prom_in);
};
