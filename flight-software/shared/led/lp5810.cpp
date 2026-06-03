#include "lp5810.hpp"

Result<void> LP5810::init(CmsisI2CBus* bus_in, uint8_t addr_in, uint8_t dc) {
    bus = bus_in;
    addr = addr_in;

    // Soft reset - returns all registers to default values
    if (auto r = write_reg(REG_RESET, CMD_RESET); !r) {
        disable();
        return r;
    }

    // Enable chip
    if (auto r = write_reg(REG_CHIP_EN, 0x01U); !r) {
        disable();
        return r;
    }

    // max_current = 0: cap at 25.5 mA (sufficient for spectroscopy illumination)
    if (auto r = write_reg(REG_CFG0, 0x00U); !r) {
        disable();
        return r;
    }

    // lsd_threshold = 3 (0x0B encodes threshold=3 per datasheet quick-start)
    if (auto r = write_reg(REG_CFG12, 0x0BU); !r) {
        disable();
        return r;
    }

    // Apply CONFIG registers (0x001-0x00D only take effect after this command)
    if (auto r = write_reg(REG_UPDATE, CMD_UPDATE); !r) {
        disable();
        return r;
    }

    // Pre-arm all 4 channels: set dot current and 100% PWM
    for (uint8_t i = 0U; i < CHANNEL_COUNT; i++) {
        if (auto r = write_reg(static_cast<uint8_t>(REG_DC0 + i), dc); !r) {
            register_failure();
            return r;
        }
        if (auto r = write_reg(static_cast<uint8_t>(REG_PWM0 + i), 0xFFU); !r) {
            register_failure();
            return r;
        }
    }

    // All LEDs off - caller controls illumination via set_channels()
    if (auto r = write_reg(REG_LED_EN, 0x00U); !r) {
        disable();
        return r;
    }

    clear_failures();
    return {};
}

Result<void> LP5810::set_channels(uint8_t mask) {
    if (is_failed()) {
        return std::unexpected(Error::DISABLED);
    }
    auto r = write_reg(REG_LED_EN, static_cast<uint8_t>(mask & 0x0FU));
    if (!r) {
        register_failure();
    }
    return r;
}

Result<void> LP5810::disable_all() {
    if (is_failed()) {
        return std::unexpected(Error::DISABLED);
    }
    auto r = write_reg(REG_LED_EN, 0x00U);
    if (!r) {
        register_failure();
    }
    return r;
}

Result<void> LP5810::write_reg(uint8_t reg, uint8_t value) {
    return bus->write_reg8(addr, reg, value);
}
