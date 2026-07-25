#include "icm42686.hpp"
#include "stm32l4xx_hal.h"
#include <array>

Result<void> ICM42686::init(CmsisI2CBus* bus, uint8_t addr, ICM42686Odr odr, bool drdy_int1) {
    this->bus = bus;
    this->addr = addr;
    this->odr_bits = static_cast<uint8_t>(odr);

    (void)this->bus->write_reg8(this->addr, REG_DEVICE_CONFIG, SOFT_RESET);
    HAL_Delay(SOFT_RESET_DELAY_MS);

    auto who = check_who_am_i();
    if (!who) {
        this->addr = (this->addr == ICM42686_ADDR) ? ICM42686_ADDR_ALT : ICM42686_ADDR;
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
    if (drdy_int1) {
        if (auto r = config_int1(); !r) {
            disable(r.error());
            return r;
        }
    }
    if (auto r = power_on(); !r) {
        disable(r.error());
        return r;
    }
    return {};
}

Result<uint8_t> ICM42686::read_who_am_i() {
    auto val = this->bus->read_reg8(this->addr, REG_WHO_AM_I);
    if (!val) {
        return mark(val.error(), Step::IMU_WHOAMI);
    }
    return *val;
}

Result<void> ICM42686::check_who_am_i() {
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

Result<void> ICM42686::config_accel() {
    const uint8_t config = (ACCEL_FS_32G << 5U) | this->odr_bits;
    if (auto r = this->bus->write_reg8(this->addr, REG_ACCEL_CONFIG0, config); !r) {
        return mark(r.error(), Step::IMU_CONFIG_ACCEL);
    }
    return {};
}

Result<void> ICM42686::config_gyro() {
    const uint8_t config = (GYRO_FS_2000 << 5U) | this->odr_bits;
    if (auto r = this->bus->write_reg8(this->addr, REG_GYRO_CONFIG0, config); !r) {
        return mark(r.error(), Step::IMU_CONFIG_GYRO);
    }
    return {};
}

Result<void> ICM42686::config_int1() {
    // INT1: pulsed, push-pull, active high -> BTC PB11 (EXTI, rising edge).
    if (auto r = this->bus->write_reg8(this->addr, REG_INT_CONFIG, INT1_PUSHPULL_ACTIVE_HIGH); !r) {
        return mark(r.error(), Step::IMU_CONFIG_INT);
    }
    // INT_ASYNC_RESET (bit 4) must be cleared from its reset value of 1
    // for proper INT1/INT2 operation (datasheet register description).
    if (auto r = this->bus->write_reg8(this->addr, REG_INT_CONFIG1, INT_ASYNC_RESET_CLEAR); !r) {
        return mark(r.error(), Step::IMU_CONFIG_INT);
    }
    if (auto r = this->bus->write_reg8(this->addr, REG_INT_SOURCE0, DRDY_TO_INT1); !r) {
        return mark(r.error(), Step::IMU_CONFIG_INT);
    }
    return {};
}

Result<void> ICM42686::power_on() {
    if (auto r = this->bus->write_reg8(this->addr, REG_PWR_MGMT0, PWR_ACCEL_GYRO); !r) {
        return mark(r.error(), Step::IMU_POWER_ON);
    }
    // Datasheet: no register writes for 200us after PWR_MGMT0.
    HAL_Delay(1U);
    return {};
}

Result<ICM42686Result> ICM42686::read_sample() {
    if (is_failed()) {
        return fail(ErrorCode::DISABLED, Step::IMU_READ, __LINE__);
    }

    uint8_t start_reg = REG_ACCEL_DATA_X1;
    std::array<uint8_t, 12> buf{};
    auto r = this->bus->write_read(this->addr, &start_reg, 1U, buf.data(), buf.size());
    if (!r) {
        const uint8_t alt = (this->addr == ICM42686_ADDR) ? ICM42686_ADDR_ALT : ICM42686_ADDR;
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
