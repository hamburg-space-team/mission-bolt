#include "lp5810.hpp"

int LP5810::init(CmsisI2CBus* bus_in, uint8_t addr_in, uint8_t dc) {
    bus = bus_in;
    addr = addr_in;

    // Soft reset - returns all registers to default values
    int ret = write_reg(REG_RESET, CMD_RESET);
    if (ret < 0) {
        disable();
        return ret;
    }

    // Enable chip
    ret = write_reg(REG_CHIP_EN, 0x01U);
    if (ret < 0) {
        disable();
        return ret;
    }

    // max_current = 0: cap at 25.5 mA (sufficient for spectroscopy illumination)
    ret = write_reg(REG_CFG0, 0x00U);
    if (ret < 0) {
        disable();
        return ret;
    }

    // lsd_threshold = 3 (0x0B encodes threshold=3 per datasheet quick-start)
    ret = write_reg(REG_CFG12, 0x0BU);
    if (ret < 0) {
        disable();
        return ret;
    }

    // Apply CONFIG registers (0x001–0x00D only take effect after this command)
    ret = write_reg(REG_UPDATE, CMD_UPDATE);
    if (ret < 0) {
        disable();
        return ret;
    }

    // Pre-arm all 4 channels: set dot current and 100% PWM
    for (uint8_t i = 0U; i < CHANNEL_COUNT; i++) {
        ret = write_reg(static_cast<uint8_t>(REG_DC0 + i), dc);
        if (ret < 0) {
            register_failure();
            return ret;
        }
        ret = write_reg(static_cast<uint8_t>(REG_PWM0 + i), 0xFFU);
        if (ret < 0) {
            register_failure();
            return ret;
        }
    }

    // All LEDs off - caller controls illumination via set_channels()
    ret = write_reg(REG_LED_EN, 0x00U);
    if (ret < 0) {
        disable();
        return ret;
    }

    clear_failures();
    return 0;
}

int LP5810::set_channels(uint8_t mask) {
    if (is_failed()) {
        return -1;
    }
    const int ret = write_reg(REG_LED_EN, static_cast<uint8_t>(mask & 0x0FU));
    if (ret < 0) {
        register_failure();
    }
    return ret;
}

int LP5810::disable_all() {
    if (is_failed()) {
        return -1;
    }
    const int ret = write_reg(REG_LED_EN, 0x00U);
    if (ret < 0) {
        register_failure();
    }
    return ret;
}

int LP5810::write_reg(uint8_t reg, uint8_t value) {
    return bus->write_reg8(addr, reg, value);
}
