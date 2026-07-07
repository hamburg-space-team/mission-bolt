#include "icm42686.hpp"
#include "stm32l4xx_hal.h"
#include <array>

Result<void> ICM42686::init(CmsisI2CBus* bus, uint8_t addr) {
    this->bus = bus;
    this->addr = addr;

    if (auto r = check_who_am_i(); !r) {
        disable();
        return r;
    }
    if (auto r = config_accel(); !r) {
        disable();
        return r;
    }
    if (auto r = config_gyro(); !r) {
        disable();
        return r;
    }
    if (auto r = power_on(); !r) {
        disable();
        return r;
    }
    return {};
}

Result<void> ICM42686::check_who_am_i() {
    auto val = this->bus->read_reg8(this->addr, REG_WHO_AM_I);
    if (!val) {
        return std::unexpected(val.error());
    }
    if (*val != EXPECTED_WHO_AM_I) {
        return std::unexpected(Error::PROTOCOL_ERROR);
    }
    return {};
}

Result<void> ICM42686::config_accel() {
    const uint8_t config = (ACCEL_FS_32G << 5U) | ODR_1KHZ;
    return this->bus->write_reg8(this->addr, REG_ACCEL_CONFIG0, config);
}

Result<void> ICM42686::config_gyro() {
    const uint8_t config = (GYRO_FS_2000 << 5U) | ODR_1KHZ;
    return this->bus->write_reg8(this->addr, REG_GYRO_CONFIG0, config);
}

Result<void> ICM42686::power_on() {
    if (auto r = this->bus->write_reg8(this->addr, REG_PWR_MGMT0, PWR_ACCEL_GYRO); !r) {
        return r;
    }
    // Datasheet: no register writes for 200us after PWR_MGMT0.
    HAL_Delay(1U);
    return {};
}

Result<ICM42686Result> ICM42686::read_sample() {
    if (is_failed()) {
        return std::unexpected(Error::DISABLED);
    }

    uint8_t start_reg = REG_ACCEL_DATA_X1;
    std::array<uint8_t, 12> buf{};
    if (auto r = this->bus->write_read(this->addr, &start_reg, 1U, buf.data(), buf.size()); !r) {
        register_failure();
        return std::unexpected(r.error());
    }

    // Shift the high byte into a uint16_t before combining with the low byte to avoid
    // implementation-defined behaviour when casting values > 0x7FFF to int16_t.
    return ICM42686Result{
        .accel_x = static_cast<int16_t>((static_cast<uint16_t>(buf[0]) << 8U) | buf[1]),
        .accel_y = static_cast<int16_t>((static_cast<uint16_t>(buf[2]) << 8U) | buf[3]),
        .accel_z = static_cast<int16_t>((static_cast<uint16_t>(buf[4]) << 8U) | buf[5]),
        .gyro_x = static_cast<int16_t>((static_cast<uint16_t>(buf[6]) << 8U) | buf[7]),
        .gyro_y = static_cast<int16_t>((static_cast<uint16_t>(buf[8]) << 8U) | buf[9]),
        .gyro_z = static_cast<int16_t>((static_cast<uint16_t>(buf[10]) << 8U) | buf[11]),
    };
}
