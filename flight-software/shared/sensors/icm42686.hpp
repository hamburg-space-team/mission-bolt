#pragma once

#include "cmsis_i2c_bus.hpp"
#include "device_base.hpp"
#include "errors.hpp"

#include <cstdint>

/// @defgroup sensors Sensor drivers

constexpr uint8_t ICM42686_ADDR = 0x69U;     // AP_AD0 = 1
constexpr uint8_t ICM42686_ADDR_ALT = 0x68U; // AP_AD0 = 0

/// One raw sample (6x 16-bit signed registers). Units depend on the
/// configured full-scale range; see config_accel() / config_gyro().
///
/// @ingroup sensors
struct ICM42686Result {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
};

/// Output data rate for accel + gyro (ODR bits of *_CONFIG0).
///
/// @ingroup sensors
enum class ICM42686Odr : uint8_t {
    ODR_1KHZ = 0x06U,
    ODR_200HZ = 0x07U,
};

/// TDK InvenSense ICM-42686-P, 6-axis IMU. Blocking driver, configures
/// +-32g accel, +-2000 dps gyro. ODR selectable; optionally routes the
/// data-ready interrupt to INT1 (pulsed, push-pull, active high).
///
/// @ingroup sensors
class ICM42686 : public DeviceBase {
  public:
    /// Verify WHO_AM_I, configure ranges, power on. Failure latches
    /// the driver via disable(). With drdy_int1 the DRDY signal is
    /// routed to INT1 (BTC: wired to PB11, EXTI rising edge).
    [[nodiscard]] Result<void> init(CmsisI2CBus* bus, uint8_t addr = ICM42686_ADDR,
                                    ICM42686Odr odr = ICM42686Odr::ODR_1KHZ, bool drdy_int1 = false);

    /// Burst-read the 12 raw register bytes.
    [[nodiscard]] Result<ICM42686Result> read_sample();

  private:
    static constexpr uint8_t REG_WHO_AM_I = 0x75U;
    static constexpr uint8_t EXPECTED_WHO_AM_I = 0x44U;
    // Cold-power-up grace: retries x delay for the first WHO_AM_I access.
    static constexpr uint8_t WHOAMI_RETRIES = 5U;
    static constexpr uint32_t WHOAMI_RETRY_DELAY_MS = 2U;
    static constexpr uint8_t REG_DEVICE_CONFIG = 0x11U;
    static constexpr uint8_t SOFT_RESET = 0x01U; // DEVICE_CONFIG bit0, wait 1 ms after
    static constexpr uint32_t SOFT_RESET_DELAY_MS = 2U;
    static constexpr uint8_t REG_PWR_MGMT0 = 0x4EU;
    static constexpr uint8_t REG_ACCEL_CONFIG0 = 0x50U;
    static constexpr uint8_t REG_GYRO_CONFIG0 = 0x4FU;
    static constexpr uint8_t REG_ACCEL_DATA_X1 = 0x1FU;
    static constexpr uint8_t REG_INT_CONFIG = 0x14U;
    static constexpr uint8_t REG_INT_CONFIG1 = 0x64U;
    static constexpr uint8_t REG_INT_SOURCE0 = 0x65U;

    // Configured full-scale ranges
    static constexpr uint8_t ACCEL_FS_32G = 0x00U;   // bits 7:5 = 000
    static constexpr uint8_t GYRO_FS_2000 = 0x01U;   // bits 7:5 = 001
    static constexpr uint8_t PWR_ACCEL_GYRO = 0x0FU; // accel+gyro low-noise mode

    // INT1 configuration (data-ready pipeline)
    static constexpr uint8_t INT1_PUSHPULL_ACTIVE_HIGH = 0x03U; // pulsed, push-pull, active high
    static constexpr uint8_t INT_ASYNC_RESET_CLEAR = 0x00U;     // bit4 must be 0 (datasheet 14.5)
    static constexpr uint8_t DRDY_TO_INT1 = 0x08U;              // INT_SOURCE0: UI_DRDY_INT1_EN

    CmsisI2CBus* bus = nullptr;
    uint8_t addr = 0U;
    uint8_t odr_bits = static_cast<uint8_t>(ICM42686Odr::ODR_1KHZ);

    [[nodiscard]] Result<void> check_who_am_i();
    [[nodiscard]] Result<void> config_accel();
    [[nodiscard]] Result<void> config_gyro();
    [[nodiscard]] Result<void> config_int1();
    [[nodiscard]] Result<void> power_on();
};
