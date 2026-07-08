#pragma once

#include "cmsis_i2c_bus.hpp"
#include "device_base.hpp"
#include "errors.hpp"

#include <array>
#include <cstdint>

constexpr uint8_t ICM42688_ADDR = 0x68U;     // AP_AD0 = 0
constexpr uint8_t ICM42688_ADDR_ALT = 0x69U; // AP_AD0 = 1

/// One raw sample. At the configured ranges: 2048 LSB/g (accel),
/// 16.384 LSB/(deg/s) (gyro).
///
/// @ingroup sensors
struct ICM42688Result {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
};

/// TDK InvenSense ICM-42688-P, 6-axis IMU. Same register map as the
/// ICM-42686-P but different WHO_AM_I and FS-scale encoding:
///   * 42688 max accel range = +-16g (vs. 42686's +-32g)
///   * 42688 gyro +-2000 dps is FS_SEL=0b000 (vs. 42686's 0b001)
/// Driver configures +-16g accel (max sensitivity), +-2000 dps gyro,
/// 1 kHz ODR.
///
/// @ingroup sensors
class ICM42688 : public DeviceBase {
  public:
    [[nodiscard]] Result<void> init(CmsisI2CBus* bus, uint8_t addr = ICM42688_ADDR);
    [[nodiscard]] Result<ICM42688Result> read_sample();

  private:
    static constexpr uint8_t REG_WHO_AM_I = 0x75U;
    static constexpr uint8_t EXPECTED_WHO_AM_I = 0x47U;
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

    // Configured full-scale ranges and ODR (max sensitivity)
    static constexpr uint8_t ACCEL_FS_16G = 0x00U;   // bits 7:5 = 000; sensitivity 2048 LSB/g
    static constexpr uint8_t GYRO_FS_2000 = 0x00U;   // bits 7:5 = 000; sensitivity 16.384 LSB/(deg/s)
    static constexpr uint8_t ODR_1KHZ = 0x06U;       // bits 3:0 = 0110
    static constexpr uint8_t PWR_ACCEL_GYRO = 0x0FU; // accel+gyro low-noise mode

    CmsisI2CBus* bus = nullptr;
    uint8_t addr = 0U;

    [[nodiscard]] Result<void> check_who_am_i();
    [[nodiscard]] Result<void> config_accel();
    [[nodiscard]] Result<void> config_gyro();
    [[nodiscard]] Result<void> power_on();
};
