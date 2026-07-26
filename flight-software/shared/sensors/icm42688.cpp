#include "icm42688.hpp"
#include "main.h" // family-correct HAL per target (L4 flight, H7 nucleo)

#include <array>

Result<void> ICM42688::init(CmsisI2CBus* bus, uint8_t addr) {
    this->bus = bus;
    this->addr = addr;

    (void)this->bus->write_reg8(this->addr, REG_DEVICE_CONFIG, SOFT_RESET);
    HAL_Delay(SOFT_RESET_DELAY_MS);

    auto who = check_who_am_i();
    if (!who) {
        this->addr = (this->addr == ICM42688_ADDR) ? ICM42688_ADDR_ALT : ICM42688_ADDR;
        (void)this->bus->write_reg8(this->addr, REG_DEVICE_CONFIG, SOFT_RESET);
        HAL_Delay(SOFT_RESET_DELAY_MS);
        who = check_who_am_i();
    }
    if (!who) {
        disable(who.error());
        return who;
    }
    if (auto r = config_accel(); !r) {
        disable(r.error());
        return r;
    }
    if (auto r = config_gyro(); !r) {
        disable(r.error());
        return r;
    }
    if (auto r = power_on(); !r) {
        disable(r.error());
        return r;
    }
    return {};
}

Result<void> ICM42688::check_who_am_i() {
    auto val = this->bus->read_reg8(this->addr, REG_WHO_AM_I);
    for (uint8_t i = 0U; (!val || *val != EXPECTED_WHO_AM_I) && i < WHOAMI_RETRIES; i++) {
        HAL_Delay(WHOAMI_RETRY_DELAY_MS);
        val = this->bus->read_reg8(this->addr, REG_WHO_AM_I);
    }
    if (!val) {
        return mark(val.error(), Step::IMU_WHOAMI);
    }
    if (*val != EXPECTED_WHO_AM_I) {
        return fail(ErrorCode::PROTOCOL_ERROR, Step::IMU_WHOAMI, __LINE__);
    }
    return {};
}

Result<void> ICM42688::config_accel() {
    const uint8_t config = (ACCEL_FS_16G << 5U) | ODR_1KHZ;
    if (auto r = this->bus->write_reg8(this->addr, REG_ACCEL_CONFIG0, config); !r) {
        return mark(r.error(), Step::IMU_CONFIG_ACCEL);
    }
    return {};
}

Result<void> ICM42688::config_gyro() {
    const uint8_t config = (GYRO_FS_2000 << 5U) | ODR_1KHZ;
    if (auto r = this->bus->write_reg8(this->addr, REG_GYRO_CONFIG0, config); !r) {
        return mark(r.error(), Step::IMU_CONFIG_GYRO);
    }
    return {};
}

Result<void> ICM42688::power_on() {
    if (auto r = this->bus->write_reg8(this->addr, REG_PWR_MGMT0, PWR_ACCEL_GYRO); !r) {
        return mark(r.error(), Step::IMU_POWER_ON);
    }
    // Datasheet: no register writes for 200us after PWR_MGMT0.
    HAL_Delay(1U);
    return {};
}

Result<ICM42688Result> ICM42688::read_sample() {
    if (is_failed()) {
        return fail(ErrorCode::DISABLED, Step::IMU_READ, __LINE__);
    }

    uint8_t start_reg = REG_ACCEL_DATA_X1;
    std::array<uint8_t, 12> buf{};
    auto r = this->bus->write_read(this->addr, &start_reg, 1U, buf.data(), buf.size());
    if (!r) {
        const uint8_t alt = (this->addr == ICM42688_ADDR) ? ICM42688_ADDR_ALT : ICM42688_ADDR;
        const auto who = this->bus->read_reg8(alt, REG_WHO_AM_I);
        if (who && *who == EXPECTED_WHO_AM_I) {
            this->addr = alt;
            r = this->bus->write_read(this->addr, &start_reg, 1U, buf.data(), buf.size());
        }
    }
    if (!r) {
        const auto marked = mark(r.error(), Step::IMU_READ);
        register_failure(marked.error());
        return marked;
    }

    return ICM42688Result{
        .accel_x = static_cast<int16_t>((static_cast<uint16_t>(buf[0]) << 8U) | buf[1]),
        .accel_y = static_cast<int16_t>((static_cast<uint16_t>(buf[2]) << 8U) | buf[3]),
        .accel_z = static_cast<int16_t>((static_cast<uint16_t>(buf[4]) << 8U) | buf[5]),
        .gyro_x = static_cast<int16_t>((static_cast<uint16_t>(buf[6]) << 8U) | buf[7]),
        .gyro_y = static_cast<int16_t>((static_cast<uint16_t>(buf[8]) << 8U) | buf[9]),
        .gyro_z = static_cast<int16_t>((static_cast<uint16_t>(buf[10]) << 8U) | buf[11]),
    };
}
