#include "cmsis_i2c_bus.hpp"

#include <array>

volatile uint32_t CmsisI2CBus::last_event = 0U;

void CmsisI2CBus::signal_event(uint32_t event) {
    last_event = event;
}

CmsisI2CBus::CmsisI2CBus(ARM_DRIVER_I2C* drv, tick_fn tick) : drv(drv), get_tick(tick) {
}

Result<void> CmsisI2CBus::init() {
    if (drv == nullptr) {
        return std::unexpected(Error::BAD_ARGUMENT);
    }
    if (drv->Initialize(&CmsisI2CBus::signal_event) != ARM_DRIVER_OK) {
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
    last_event = 0U;
    if (drv->MasterTransmit(addr, data, len, false) != ARM_DRIVER_OK) {
        return std::unexpected(Error::BUS_ERROR);
    }
    return wait_complete();
}

Result<void> CmsisI2CBus::read(uint8_t addr, uint8_t* data, std::size_t len) {
    if (drv == nullptr) {
        return std::unexpected(Error::BAD_ARGUMENT);
    }
    last_event = 0U;
    if (drv->MasterReceive(addr, data, len, false) != ARM_DRIVER_OK) {
        return std::unexpected(Error::BUS_ERROR);
    }
    return wait_complete();
}

Result<void> CmsisI2CBus::write_read(uint8_t addr, const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                                     std::size_t rx_len) {
    if (drv == nullptr) {
        return std::unexpected(Error::BAD_ARGUMENT);
    }

    last_event = 0U;
    if (drv->MasterTransmit(addr, tx, tx_len, true) != ARM_DRIVER_OK) {
        return std::unexpected(Error::BUS_ERROR);
    }
    if (auto r = wait_complete(); !r) {
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

Result<void> CmsisI2CBus::wait_complete() const {
    if (get_tick != nullptr) {
        const uint32_t start = get_tick();
        while ((last_event & ARM_I2C_EVENT_TRANSFER_DONE) == 0U) {
            if ((get_tick() - start) >= I2C_TIMEOUT_MS) {
                return std::unexpected(Error::TIMEOUT);
            }
        }
    } else {
        uint32_t i = 0U;
        while ((last_event & ARM_I2C_EVENT_TRANSFER_DONE) == 0U) {
            if (++i >= BUSY_TIMEOUT) {
                return std::unexpected(Error::TIMEOUT);
            }
        }
    }

    const uint32_t event = last_event;
    if ((event & (ARM_I2C_EVENT_ADDRESS_NACK | ARM_I2C_EVENT_TRANSFER_INCOMPLETE)) != 0U) {
        return std::unexpected(Error::PROTOCOL_ERROR);
    }
    if ((event & (ARM_I2C_EVENT_BUS_ERROR | ARM_I2C_EVENT_ARBITRATION_LOST)) != 0U) {
        return std::unexpected(Error::BUS_ERROR);
    }
    return {};
}