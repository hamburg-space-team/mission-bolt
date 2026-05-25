#include "cmsis_i2c_bus.hpp"

#include <array>

CmsisI2CBus::CmsisI2CBus(ARM_DRIVER_I2C* drv, tick_fn tick) : drv(drv), get_tick(tick) {
}

int CmsisI2CBus::init() {
    if (drv == nullptr) {
        return -1;
    }
    if (drv->Initialize(nullptr) != ARM_DRIVER_OK) {
        return -1;
    }
    if (drv->PowerControl(ARM_POWER_FULL) != ARM_DRIVER_OK) {
        return -1;
    }
    if (drv->Control(ARM_I2C_BUS_SPEED, ARM_I2C_BUS_SPEED_FAST) != ARM_DRIVER_OK) {
        return -1;
    }
    return 0;
}

int CmsisI2CBus::write(uint8_t addr, const uint8_t* data, size_t len) {
    if (drv == nullptr) {
        return -1;
    }
    if (drv->MasterTransmit(addr, data, len, false) != ARM_DRIVER_OK) {
        return -1;
    }
    return wait_busy();
}

int CmsisI2CBus::read(uint8_t addr, uint8_t* data, size_t len) {
    if (drv == nullptr) {
        return -1;
    }
    if (drv->MasterReceive(addr, data, len, false) != ARM_DRIVER_OK) {
        return -1;
    }
    return wait_busy();
}

int CmsisI2CBus::write_read(uint8_t addr, const uint8_t* tx, size_t tx_len, uint8_t* rx, size_t rx_len) {
    const int ret = write(addr, tx, tx_len);
    if (ret < 0) {
        return ret;
    }
    return read(addr, rx, rx_len);
}

int CmsisI2CBus::write_reg8(uint8_t addr, uint8_t reg, uint8_t value) {
    const std::array<uint8_t, 2> buf = {reg, value};
    return write(addr, buf.data(), buf.size());
}

int CmsisI2CBus::write_reg16(uint8_t addr, uint8_t reg, uint16_t value) {
    const std::array<uint8_t, 3> buf = {reg, static_cast<uint8_t>(value >> 8U), static_cast<uint8_t>(value & 0xFFU)};
    return write(addr, buf.data(), buf.size());
}

int CmsisI2CBus::read_reg8(uint8_t addr, uint8_t reg, uint8_t* value) {
    uint8_t buf = 0U;
    const int ret = write_read(addr, &reg, 1U, &buf, 1U);
    if (ret < 0) {
        return ret;
    }
    *value = buf;
    return 0;
}

int CmsisI2CBus::read_reg16(uint8_t addr, uint8_t reg, uint16_t* value) {
    std::array<uint8_t, 2> buf = {};
    const int ret = write_read(addr, &reg, 1U, buf.data(), buf.size());
    if (ret < 0) {
        return ret;
    }
    *value = static_cast<uint16_t>((static_cast<uint16_t>(buf[0]) << 8U) | static_cast<uint16_t>(buf[1]));
    return 0;
}

int CmsisI2CBus::wait_busy() const {
    if (get_tick != nullptr) {
        const uint32_t start = get_tick();
        while ((get_tick() - start) < I2C_TIMEOUT_MS) {
            if (drv->GetStatus().busy == 0U) {
                return 0;
            }
        }
        return -1;
    }
    for (uint32_t i = 0U; i < BUSY_TIMEOUT; i++) {
        if (drv->GetStatus().busy == 0U) {
            return 0;
        }
    }
    return -1;
}
