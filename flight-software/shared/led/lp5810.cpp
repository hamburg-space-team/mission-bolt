#include "lp5810.hpp"

Result<void> LP5810::init(CmsisI2CBus* bus_in, uint8_t addr_in, uint8_t dc, delay_fn delay, bool high_current) {
    this->bus = bus_in;
    this->addr = addr_in;
    this->dot_current = dc;
    this->max_current = high_current;
    this->delay_ms = delay;

    if (auto r = configure(); !r) {
        const auto marked = mark(r.error(), Step::LED_INIT);
        disable(marked.error());
        return marked;
    }

    clear_failures();
    return {};
}

// Full power-on register sequence. Used by init() and re-run from recover()
// after a bus reset to restore the chip's state.
Result<void> LP5810::configure() {
    (void)write_reg(REG_RESET, CMD_RESET);
    if (this->delay_ms != nullptr) {
        this->delay_ms(RESET_BOOT_DELAY_MS);
    }

    if (auto r = enable_chip(); !r) {
        return r;
    }

    // CFG0.max_current selects the full-scale: 0 = 25.5 mA, 1 = 51 mA.
    if (auto r = write_reg(REG_CFG0, this->max_current ? 0x01U : 0x00U); !r) {
        return mark(r.error(), Step::LED_CONFIGURE);
    }

    // lsd_threshold = 3 (0x0B encodes threshold=3 per datasheet quick-start)
    if (auto r = write_reg(REG_CFG12, 0x0BU); !r) {
        return mark(r.error(), Step::LED_CONFIGURE);
    }

    // Apply CONFIG registers (0x001-0x00D only take effect after this command)
    if (auto r = write_reg(REG_UPDATE, CMD_UPDATE); !r) {
        return mark(r.error(), Step::LED_CONFIGURE);
    }

    // Pre-arm all 4 channels: set dot current and 100% PWM
    for (uint8_t i = 0U; i < CHANNEL_COUNT; i++) {
        if (auto r = write_reg(static_cast<uint8_t>(REG_DC0 + i), this->dot_current); !r) {
            return mark(r.error(), Step::LED_CONFIGURE);
        }
        if (auto r = write_reg(static_cast<uint8_t>(REG_PWM0 + i), 0xFFU); !r) {
            return mark(r.error(), Step::LED_CONFIGURE);
        }
    }
    // All LEDs off - caller controls illumination via set_channels()
    if (auto r = write_reg(REG_LED_EN, 0x00U); !r) {
        return mark(r.error(), Step::LED_CONFIGURE);
    }
    return {};
}

Result<void> LP5810::set_channels(uint8_t mask, uint8_t pwm) {
    if (is_failed()) {
        return fail(ErrorCode::DISABLED, Step::LED_SET_CHANNELS, __LINE__);
    }
    if (auto r = apply_channels(mask, pwm)) {
        return r;
    }

    // A runtime write failed mid-sequence. Same recovery idea as
    // recover(reg, value), but the whole multi-register step (PWM0-3 +
    // LED_EN) is re-applied after the re-init - a partially applied
    // brightness must never be reported as success.
    (void)this->bus->reset();
    Result<void> r = configure();
    if (r) {
        r = apply_channels(mask, pwm);
    }
    if (r) {
        clear_failures();
        return r;
    }
    const auto marked = mark(r.error(), Step::LED_SET_CHANNELS);
    register_failure(marked.error());
    return marked;
}

Result<void> LP5810::apply_channels(uint8_t mask, uint8_t pwm) {
    for (uint8_t i = 0U; i < CHANNEL_COUNT; i++) {
        if (auto r = write_reg(static_cast<uint8_t>(REG_PWM0 + i), pwm); !r) {
            return mark(r.error(), Step::LED_APPLY);
        }
    }
    if (auto r = write_reg(REG_LED_EN, static_cast<uint8_t>(mask & 0x0FU)); !r) {
        return mark(r.error(), Step::LED_APPLY);
    }
    return {};
}

Result<void> LP5810::disable_all() {
    if (is_failed()) {
        return fail(ErrorCode::DISABLED, Step::LED_DISABLE_ALL, __LINE__);
    }
    if (auto r = write_reg(REG_LED_EN, 0x00U)) {
        return r;
    }
    if (auto r = recover(REG_LED_EN, 0x00U); !r) {
        return mark(r.error(), Step::LED_DISABLE_ALL);
    }
    return {};
}

// write_reg is a transparent pass-through to the bus (ADR-012).
Result<void> LP5810::write_reg(uint8_t reg, uint8_t value) {
    return this->bus->write_reg8(this->addr, reg, value);
}

// Write chip_en = 1 (0x000): moves the device INITIAL -> STANDBY (datasheet 9.2.3.2 step 2 / Figure 7-9).
Result<void> LP5810::enable_chip() {
    Result<void> r = fail(ErrorCode::PROTOCOL_ERROR, Step::LED_ENABLE_CHIP, __LINE__);
    for (uint8_t attempt = 0U; attempt < CHIP_EN_WRITE_RETRIES; attempt++) {
        r = write_reg(REG_CHIP_EN, 0x01U);
        if (r) {
            return r;
        }
        if (this->delay_ms != nullptr) {
            this->delay_ms(CHIP_EN_RETRY_DELAY_MS);
        }
    }
    
    // Retries exhausted - r holds the last write's error chain.
    return mark(r.error(), Step::LED_ENABLE_CHIP);
}

// A runtime write failed (NACK / wedged controller).
// Run the full recovery path before giving up
Result<void> LP5810::recover(uint8_t reg, uint8_t value) {
    (void)this->bus->reset();
    Result<void> r = configure();
    if (r) {
        r = write_reg(reg, value);
    }
    if (r) {
        clear_failures();
        return r;
    }
    const auto marked = mark(r.error(), Step::LED_RECOVER);
    register_failure(marked.error());
    return marked;
}
