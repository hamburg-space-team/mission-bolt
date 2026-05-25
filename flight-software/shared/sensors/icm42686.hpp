#pragma once

#include "cmsis_i2c_bus.hpp"
#include "device_base.hpp"

#include <array>
#include <cstdint>

constexpr uint8_t ICM42686_ADDR = 0x68U;

struct ICM42686Result {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
};

class ICM42686 : public DeviceBase {
  public:
    [[nodiscard]] int init(CmsisI2CBus* bus, uint8_t addr = ICM42686_ADDR);
    [[nodiscard]] int read_sample(ICM42686Result* result);

  private:
    static constexpr uint8_t REG_WHO_AM_I = 0x75U;
    static constexpr uint8_t EXPECTED_WHO_AM_I = 0x44U;
    static constexpr uint8_t REG_PWR_MGMT0 = 0x4EU;
    static constexpr uint8_t REG_ACCEL_CONFIG0 = 0x50U;
    static constexpr uint8_t REG_GYRO_CONFIG0 = 0x4FU;
    static constexpr uint8_t REG_ACCEL_DATA_X1 = 0x1FU;

    // Configured full-scale ranges and ODR
    static constexpr uint8_t ACCEL_FS_32G = 0x00U;   // bits 7:5 = 000
    static constexpr uint8_t GYRO_FS_2000 = 0x01U;   // bits 7:5 = 001
    static constexpr uint8_t ODR_1KHZ = 0x06U;       // bits 3:0 = 0110
    static constexpr uint8_t PWR_ACCEL_GYRO = 0x0FU; // accel+gyro low-noise mode

    CmsisI2CBus* bus = nullptr;
    uint8_t addr = 0U;

    [[nodiscard]] int check_who_am_i();
    [[nodiscard]] int config_accel();
    [[nodiscard]] int config_gyro();
    [[nodiscard]] int power_on();
};
