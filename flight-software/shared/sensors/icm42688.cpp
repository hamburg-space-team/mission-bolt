#include "icm42688.hpp"
#include "main.h"

int ICM42688::init(CmsisI2CBus* bus, uint8_t addr) {
    this->bus = bus;
    this->addr = addr;

    if (check_who_am_i() < 0 || config_accel() < 0 || config_gyro() < 0 || power_on() < 0) {
        disable();
        return -1;
    }

    return 0;
}

int ICM42688::check_who_am_i() {
    uint8_t reg = REG_WHO_AM_I;
    uint8_t val = 0U;
    const auto ret = bus->write_read(addr, &reg, 1U, &val, 1U);

    if (ret < 0 || val != EXPECTED_WHO_AM_I) {
        return -1;
    }

    return 0;
}

int ICM42688::config_accel() {
    const uint8_t config = (ACCEL_FS_16G << 5U) | ODR_1KHZ;
    std::array<uint8_t, 2> buf = {REG_ACCEL_CONFIG0, config};

    return bus->write(addr, buf.data(), buf.size());
}

int ICM42688::config_gyro() {
    const uint8_t config = (GYRO_FS_2000 << 5U) | ODR_1KHZ;
    std::array<uint8_t, 2> buf = {REG_GYRO_CONFIG0, config};

    return bus->write(addr, buf.data(), buf.size());
}

int ICM42688::power_on() {
    std::array<uint8_t, 2> buf = {REG_PWR_MGMT0, PWR_ACCEL_GYRO};
    const int ret = bus->write(addr, buf.data(), buf.size());
    if (ret < 0) {
        return ret;
    }

    // Datasheet: no register writes for 200µs after PWR_MGMT0.
    HAL_Delay(1U);
    return 0;
}

int ICM42688::read_sample(ICM42688Result* result) {
    if (is_failed() || result == nullptr) {
        return -1;
    }

    uint8_t start_reg = REG_ACCEL_DATA_X1;
    std::array<uint8_t, 12> buf{};
    const int ret = bus->write_read(addr, &start_reg, 1U, buf.data(), buf.size());
    if (ret < 0) {
        register_failure();
        return ret;
    }

    // Shift the high byte into a uint16_t before combining with the low byte to avoid
    // implementation-defined behaviour when casting values > 0x7FFF to int16_t.
    result->accel_x = static_cast<int16_t>((static_cast<uint16_t>(buf[0]) << 8U) | buf[1]);
    result->accel_y = static_cast<int16_t>((static_cast<uint16_t>(buf[2]) << 8U) | buf[3]);
    result->accel_z = static_cast<int16_t>((static_cast<uint16_t>(buf[4]) << 8U) | buf[5]);
    result->gyro_x = static_cast<int16_t>((static_cast<uint16_t>(buf[6]) << 8U) | buf[7]);
    result->gyro_y = static_cast<int16_t>((static_cast<uint16_t>(buf[8]) << 8U) | buf[9]);
    result->gyro_z = static_cast<int16_t>((static_cast<uint16_t>(buf[10]) << 8U) | buf[11]);
    return 0;
}