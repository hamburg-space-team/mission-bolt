#include "cmsis_i2c_bus.hpp"

#include <array>

CmsisI2CBus::CmsisI2CBus(ARM_DRIVER_I2C* drv, tick_fn tick) : drv(drv), get_tick(tick) {
}

Result<void> CmsisI2CBus::init() {
    if (drv == nullptr) {
        return std::unexpected(Error::BAD_ARGUMENT);
    }
    if (drv->Initialize(nullptr) != ARM_DRIVER_OK) {
        return std::unexpected(Error::BUS_ERROR);
    }
    if (drv->PowerControl(ARM_POWER_FULL) != ARM_DRIVER_OK) {
        return std::unexpected(Error::BUS_ERROR);
    }
    if (drv->Control(ARM_I2C_BUS_SPEED, ARM_I2C_BUS_SPEED_FAST) != ARM_DRIVER_OK) {
        return std::unexpected(Error::BUS_ERROR);
    }
    return {};
}

Result<void> CmsisI2CBus::write(uint8_t addr, const uint8_t* data, std::size_t len) {
    if (drv == nullptr) {
        return std::unexpected(Error::BAD_ARGUMENT);
    }
    if (drv->MasterTransmit(addr, data, len, false) != ARM_DRIVER_OK) {
        return std::unexpected(Error::BUS_ERROR);
    }
    return wait_busy();
}

Result<void> CmsisI2CBus::read(uint8_t addr, uint8_t* data, std::size_t len) {
    if (drv == nullptr) {
        return std::unexpected(Error::BAD_ARGUMENT);
    }
    if (drv->MasterReceive(addr, data, len, false) != ARM_DRIVER_OK) {
        return std::unexpected(Error::BUS_ERROR);
    }
    return wait_busy();
}

Result<void> CmsisI2CBus::write_read(uint8_t addr, const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                                     std::size_t rx_len) {
    if (drv == nullptr) {
        return std::unexpected(Error::BAD_ARGUMENT);
    }

    if (drv->MasterTransmit(addr, tx, tx_len, true) != ARM_DRIVER_OK) {
        return std::unexpected(Error::BUS_ERROR);
    }
    if (auto r = wait_busy(); !r) {
        return r;
    }

    return read(addr, rx, rx_len);
}

Result<void> CmsisI2CBus::write_reg8(uint8_t addr, uint8_t reg, uint8_t value) {
    const std::array<uint8_t, 2> buf = {reg, value};
    return write(addr, buf.data(), buf.size());
}

Result<void> CmsisI2CBus::write_reg16(uint8_t addr, uint8_t reg, uint16_t value) {
    const std::array<uint8_t, 3> buf = {reg, static_cast<uint8_t>(value >> 8U), static_cast<uint8_t>(value & 0xFFU)};
    return write(addr, buf.data(), buf.size());
}

Result<uint8_t> CmsisI2CBus::read_reg8(uint8_t addr, uint8_t reg) {
    uint8_t buf = 0U;
    if (auto r = write_read(addr, &reg, 1U, &buf, 1U); !r) {
        return std::unexpected(r.error());
    }
    return buf;
}

Result<uint16_t> CmsisI2CBus::read_reg16(uint8_t addr, uint8_t reg) {
    std::array<uint8_t, 2> buf = {};
    if (auto r = write_read(addr, &reg, 1U, buf.data(), buf.size()); !r) {
        return std::unexpected(r.error());
    }
    return static_cast<uint16_t>((static_cast<uint16_t>(buf[0]) << 8U) | static_cast<uint16_t>(buf[1]));
}

Result<void> CmsisI2CBus::wait_busy() const {
    if (get_tick != nullptr) {
        const uint32_t start = get_tick();
        while ((get_tick() - start) < I2C_TIMEOUT_MS) {
            if (drv->GetStatus().busy == 0U) {
                return {};
            }
        }
        return std::unexpected(Error::TIMEOUT);
    }
    for (uint32_t i = 0U; i < BUSY_TIMEOUT; i++) {
        if (drv->GetStatus().busy == 0U) {
            return {};
        }
    }
    return std::unexpected(Error::TIMEOUT);
}
