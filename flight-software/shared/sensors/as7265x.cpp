#include "as7265x.hpp"

static constexpr uint8_t REG_WRITE = 0x01U;

Result<void> AS7265X::init(CmsisI2CBus* bus_in, tick_fn tick_in, uint8_t addr_in, uint8_t integration_cycles,
                           AS7265XGain gain, data_rdy_fn int_pin) {
    this->bus = bus_in;
    this->tick = tick_in;
    this->addr = addr_in;
    this->data_rdy_pin = int_pin;

    if (auto r = write_virtual(VREG_INT_TIME, integration_cycles); !r) {
        disable();
        return r;
    }
    this->current_cycles = integration_cycles;

    // Gain at bits [5:4]; enable hardware INT if pin provided; leave BANK=0 (no measurement yet).
    auto ctrl = static_cast<uint8_t>(static_cast<uint8_t>(gain) << 4U);
    if (int_pin != nullptr) {
        ctrl = static_cast<uint8_t>(ctrl | INT_ENABLE_BIT);
    }
    if (auto r = write_virtual(VREG_CONTROL, ctrl); !r) {
        disable();
        return r;
    }

    clear_failures();
    return {};
}

Result<void> AS7265X::start_measurement() {
    if (is_failed()) {
        return std::unexpected(Error::DISABLED);
    }

    auto ctrl = read_virtual(VREG_CONTROL);
    if (!ctrl) {
        register_failure();
        return std::unexpected(ctrl.error());
    }

    // Set BANK bits [3:2] to one-shot, preserve gain and INT bits.
    const auto new_ctrl = static_cast<uint8_t>((*ctrl & 0xF3U) | static_cast<uint8_t>(MODE_ONE_SHOT << 2U));
    if (auto r = write_virtual(VREG_CONTROL, new_ctrl); !r) {
        register_failure();
        return r;
    }
    return {};
}

Result<void> AS7265X::set_integration(uint8_t cycles) {
    if (is_failed()) {
        return std::unexpected(Error::DISABLED);
    }
    if (cycles == this->current_cycles) {
        return {};
    }
    if (auto r = write_virtual(VREG_INT_TIME, cycles); !r) {
        register_failure();
        return r;
    }
    this->current_cycles = cycles;
    return {};
}

bool AS7265X::data_ready() {
    if (is_failed()) {
        return false;
    }
    if (this->data_rdy_pin != nullptr) {
        return this->data_rdy_pin();
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
        return std::unexpected(Error::BAD_ARGUMENT);
    }
    if (is_failed()) {
        return std::unexpected(Error::DISABLED);
    }

    // The AS7265x is three sensor dies behind one master. The RAW window
    // 0x08-0x13 only ever shows the die selected via VREG_DEV_SEL; the bytes
    // from 0x14 on are the calibrated IEEE-754 floats. Reading 18 channels
    // linearly from 0x08 therefore returned floats-as-uint16 garbage for
    // channels 6-17 (issue #24). Correct: select each die, read its six RAW
    // channels. Result order: [0..5] = die 0 (AS72651), [6..11] = die 1
    // (AS72652), [12..17] = die 2 (AS72653); wavelength mapping happens on
    // ground (ADR-009 / ICD-007).
    for (uint8_t dev = first_die; dev < first_die + die_count; dev++) {
        if (auto r = write_virtual(VREG_DEV_SEL, dev); !r) {
            register_failure();
            return r;
        }
        for (uint8_t i = 0U; i < CHANNELS_PER_DEVICE; i++) {
            const auto vreg_hi = static_cast<uint8_t>(FIRST_CHANNEL_VREG + i * 2U);
            const auto vreg_lo = static_cast<uint8_t>(FIRST_CHANNEL_VREG + i * 2U + 1U);

            auto hi = read_virtual(vreg_hi);
            if (!hi) {
                register_failure();
                return std::unexpected(hi.error());
            }
            auto lo = read_virtual(vreg_lo);
            if (!lo) {
                register_failure();
                return std::unexpected(lo.error());
            }

            result->channels[(dev * CHANNELS_PER_DEVICE) + i] =
                (static_cast<uint16_t>(*hi) << 8U) | static_cast<uint16_t>(*lo);
        }
    }

    // Restore die 0 so the master-level accesses elsewhere in the driver
    // (VREG_CONTROL polling, start_measurement) see the same device state
    // as after boot.
    if (auto r = write_virtual(VREG_DEV_SEL, 0U); !r) {
        register_failure();
        return r;
    }

    clear_failures();
    return {};
}

// Poll STATUS until `done(status)` is true. The register pointer persists
// between transactions on the AS72651, so only the FIRST poll pays for the
// pointer write (write_read); every further poll is a bare 1-byte read at
// roughly half the bus time. With ~8 polls per virtual-register access the
// polling dominates the read cost, so this halves most of it (issue #24).
Result<void> AS7265X::wait_status(uint8_t mask, uint8_t want) {
    const uint32_t deadline = (this->tick != nullptr) ? (this->tick() + TIMEOUT_MS) : 0U;

    auto status = this->bus->read_reg8(this->addr, REG_STATUS); // sets the pointer
    for (uint32_t i = 0U; i < BUSY_ITER; i++) {
        if (!status) {
            return std::unexpected(status.error());
        }
        if ((*status & mask) == want) {
            return {};
        }
        if (this->tick != nullptr && this->tick() >= deadline) {
            return std::unexpected(Error::TIMEOUT);
        }
        uint8_t raw = 0U;
        if (auto r = this->bus->read(this->addr, &raw, 1U); !r) { // pointer still on STATUS
            return std::unexpected(r.error());
        }
        status = raw;
    }
    return std::unexpected(Error::TIMEOUT);
}

Result<void> AS7265X::wait_tx_ready() {
    return wait_status(STATUS_TX_FULL, 0U);
}

Result<void> AS7265X::wait_rx_ready() {
    return wait_status(STATUS_RX_VALID, STATUS_RX_VALID);
}

Result<void> AS7265X::write_virtual(uint8_t vreg, uint8_t value) {
    if (auto r = wait_tx_ready(); !r) {
        return r;
    }
    if (auto r = this->bus->write_reg8(this->addr, REG_WRITE, static_cast<uint8_t>(vreg | VWRITE_FLAG)); !r) {
        return r;
    }
    if (auto r = wait_tx_ready(); !r) {
        return r;
    }
    return this->bus->write_reg8(this->addr, REG_WRITE, value);
}

Result<uint8_t> AS7265X::read_virtual(uint8_t vreg) {
    if (auto r = wait_tx_ready(); !r) {
        return std::unexpected(r.error());
    }
    if (auto r = this->bus->write_reg8(this->addr, REG_WRITE, vreg); !r) {
        return std::unexpected(r.error());
    }
    if (auto r = wait_rx_ready(); !r) {
        return std::unexpected(r.error());
    }
    return this->bus->read_reg8(this->addr, REG_READ);
}
