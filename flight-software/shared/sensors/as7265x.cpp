#include "as7265x.hpp"

static constexpr uint8_t REG_WRITE = 0x01U;

Result<void> AS7265X::init(CmsisI2CBus* bus_in, tick_fn tick_in, uint8_t addr_in, uint8_t integration_cycles,
                           AS7265XGain gain, data_rdy_fn int_pin, delay_fn delay) {
    this->bus = bus_in;
    this->tick = tick_in;
    this->addr = addr_in;
    this->data_rdy_pin = int_pin;
    this->delay_ms = delay;

    if (auto r = write_virtual(VREG_INT_TIME, integration_cycles); !r) {
        const auto marked = mark(r.error(), Step::SPEC_INIT);
        disable(marked.error());
        return marked;
    }
    this->current_cycles = integration_cycles;

    // Gain at bits [5:4]; enable hardware INT if pin provided; leave BANK=0 (no measurement yet).
    auto ctrl = static_cast<uint8_t>(static_cast<uint8_t>(gain) << 4U);
    if (int_pin != nullptr) {
        ctrl = static_cast<uint8_t>(ctrl | INT_ENABLE_BIT);
    }
    if (auto r = write_virtual(VREG_CONTROL, ctrl); !r) {
        const auto marked = mark(r.error(), Step::SPEC_INIT);
        disable(marked.error());
        return marked;
    }

    clear_failures();
    return {};
}

Result<void> AS7265X::start_measurement() {
    if (is_failed()) {
        return fail(ErrorCode::DISABLED, Step::SPEC_START_MEAS, __LINE__);
    }

    auto ctrl = read_virtual(VREG_CONTROL);
    if (!ctrl) {
        const auto marked = mark(ctrl.error(), Step::SPEC_START_MEAS);
        register_failure(marked.error());
        return marked;
    }

    // Set BANK bits [3:2] to one-shot, preserve gain and INT bits.
    const auto new_ctrl = static_cast<uint8_t>((*ctrl & 0xF3U) | static_cast<uint8_t>(MODE_ONE_SHOT << 2U));
    if (auto r = write_virtual(VREG_CONTROL, new_ctrl); !r) {
        const auto marked = mark(r.error(), Step::SPEC_START_MEAS);
        register_failure(marked.error());
        return marked;
    }
    return {};
}

Result<void> AS7265X::set_integration(uint8_t cycles) {
    if (is_failed()) {
        return fail(ErrorCode::DISABLED, Step::SPEC_SET_INTEGRATION, __LINE__);
    }
    if (cycles == this->current_cycles) {
        return {};
    }
    if (auto r = write_virtual(VREG_INT_TIME, cycles); !r) {
        const auto marked = mark(r.error(), Step::SPEC_SET_INTEGRATION);
        register_failure(marked.error());
        return marked;
    }
    this->current_cycles = cycles;
    return {};
}

bool AS7265X::data_ready() {
    if (is_failed()) {
        return false;
    }
    if (this->data_rdy_pin != nullptr && this->data_rdy_pin()) {
        return true;
    }
    auto ctrl = read_virtual(VREG_CONTROL);
    if (!ctrl) {
        return false;
    }
    return (*ctrl & DATA_RDY_BIT) != 0U;
}

Result<void> AS7265X::read_channels(AS7265XResult* result) {
    return read_channels_dies(result, 0U, DEVICE_COUNT);
}

Result<void> AS7265X::read_channels_dies(AS7265XResult* result, uint8_t first_die, uint8_t die_count) {
    if (result == nullptr || (static_cast<uint16_t>(first_die) + die_count) > DEVICE_COUNT) {
        return fail(ErrorCode::BAD_ARGUMENT, Step::SPEC_READ_DIES, __LINE__);
    }
    if (is_failed()) {
        return fail(ErrorCode::DISABLED, Step::SPEC_READ_DIES, __LINE__);
    }

    for (uint8_t dev = first_die; dev < first_die + die_count; dev++) {
        if (auto r = write_virtual(VREG_DEV_SEL, dev); !r) {
            const auto marked = mark(r.error(), Step::SPEC_DEV_SEL);
            register_failure(marked.error());
            return marked;
        }
        for (uint8_t i = 0U; i < CHANNELS_PER_DEVICE; i++) {
            const auto vreg_hi = static_cast<uint8_t>(FIRST_CHANNEL_VREG + i * 2U);
            const auto vreg_lo = static_cast<uint8_t>(FIRST_CHANNEL_VREG + i * 2U + 1U);

            auto hi = read_virtual(vreg_hi);
            if (!hi) {
                const auto marked = mark(hi.error(), Step::SPEC_READ_DIES);
                register_failure(marked.error());
                return marked;
            }
            auto lo = read_virtual(vreg_lo);
            if (!lo) {
                const auto marked = mark(lo.error(), Step::SPEC_READ_DIES);
                register_failure(marked.error());
                return marked;
            }

            result->channels[(dev * CHANNELS_PER_DEVICE) + i] =
                (static_cast<uint16_t>(*hi) << 8U) | static_cast<uint16_t>(*lo);
        }
    }

    // Restore die 0 so the master-level accesses elsewhere in the driver
    // (VREG_CONTROL polling, start_measurement) see the same device state
    // as after boot.
    if (auto r = write_virtual(VREG_DEV_SEL, 0U); !r) {
        const auto marked = mark(r.error(), Step::SPEC_DEV_SEL);
        register_failure(marked.error());
        return marked;
    }

    clear_failures();
    return {};
}

Result<uint8_t> AS7265X::hw_read_reg(uint8_t reg) {
    if (auto r = this->bus->write(this->addr, &reg, 1U); !r) {
        return std::unexpected(r.error());
    }
    uint8_t value = 0U;
    if (auto r = this->bus->read(this->addr, &value, 1U); !r) {
        return std::unexpected(r.error());
    }
    return value;
}

Result<void> AS7265X::wait_status(uint8_t mask, uint8_t want) {
    const bool have_clock = (this->tick != nullptr);
    const uint32_t deadline = have_clock ? (this->tick() + TIMEOUT_MS) : 0U;

    for (uint32_t i = 0U; have_clock || i < BUSY_ITER; i++) {
        const auto status = hw_read_reg(REG_STATUS);
        if (!status) {
            return mark(status.error(), Step::SPEC_WAIT_STATUS);
        }
        if ((*status & mask) == want) {
            return {};
        }
        if (have_clock && this->tick() >= deadline) {
            break;
        }
    }
    return fail(ErrorCode::TIMEOUT, Step::SPEC_WAIT_STATUS, __LINE__);
}

Result<void> AS7265X::wait_tx_ready() {
    return wait_status(STATUS_TX_FULL, 0U);
}

Result<void> AS7265X::wait_rx_ready() {
    return wait_status(STATUS_RX_VALID, STATUS_RX_VALID);
}

Result<void> AS7265X::write_virtual(uint8_t vreg, uint8_t value) {
    if (auto r = wait_tx_ready(); !r) {
        return mark(r.error(), Step::SPEC_VREG_WRITE);
    }
    if (auto r = this->bus->write_reg8(this->addr, REG_WRITE, static_cast<uint8_t>(vreg | VWRITE_FLAG)); !r) {
        return mark(r.error(), Step::SPEC_VREG_WRITE);
    }
    if (auto r = wait_tx_ready(); !r) {
        return mark(r.error(), Step::SPEC_VREG_WRITE);
    }
    if (auto r = this->bus->write_reg8(this->addr, REG_WRITE, value); !r) {
        return mark(r.error(), Step::SPEC_VREG_WRITE);
    }
    return {};
}

Result<void> AS7265X::probe(CmsisI2CBus* bus_in, tick_fn tick_in, delay_fn delay) {
    this->bus = bus_in;
    this->tick = tick_in;
    this->delay_ms = delay;
    this->addr = AS7265X_ADDR;
    if (auto r = read_virtual(VREG_HW_VERSION); !r) {
        return std::unexpected(r.error());
    }
    return {};
}

Result<uint8_t> AS7265X::hw_version() {
    if (this->bus == nullptr) {
        return fail(ErrorCode::DISABLED, Step::SPEC_VREG_READ, __LINE__);
    }
    return read_virtual(VREG_HW_VERSION);
}

Result<uint8_t> AS7265X::read_virtual(uint8_t vreg) {
    if (auto status = hw_read_reg(REG_STATUS); status && ((*status & STATUS_RX_VALID) != 0U)) {
        (void)hw_read_reg(REG_READ);
    }
    if (auto r = wait_tx_ready(); !r) {
        return mark(r.error(), Step::SPEC_VREG_READ);
    }
    if (auto r = this->bus->write_reg8(this->addr, REG_WRITE, vreg); !r) {
        return mark(r.error(), Step::SPEC_VREG_READ);
    }
    if (auto r = wait_rx_ready(); !r) {
        return mark(r.error(), Step::SPEC_VREG_READ);
    }
    auto value = hw_read_reg(REG_READ);
    if (!value) {
        return mark(value.error(), Step::SPEC_VREG_READ);
    }
    return value;
}
