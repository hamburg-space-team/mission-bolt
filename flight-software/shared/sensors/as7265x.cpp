#include "as7265x.hpp"

static constexpr uint8_t REG_WRITE = 0x01U;

Result<void> AS7265X::init(CmsisI2CBus* bus_in, tick_fn tick_in, uint8_t addr_in, uint8_t integration_cycles,
                           AS7265XGain gain, data_rdy_fn int_pin) {
    bus = bus_in;
    tick = tick_in;
    addr = addr_in;
    data_rdy_pin = int_pin;

    if (auto r = write_virtual(VREG_INT_TIME, integration_cycles); !r) {
        disable();
        return r;
    }

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

bool AS7265X::data_ready() {
    if (is_failed()) {
        return false;
    }
    if (data_rdy_pin != nullptr) {
        return data_rdy_pin();
    }
    auto ctrl = read_virtual(VREG_CONTROL);
    if (!ctrl) {
        return false;
    }
    return (*ctrl & DATA_RDY_BIT) != 0U;
}

Result<void> AS7265X::read_channels(AS7265XResult* result) {
    if (result == nullptr) {
        return std::unexpected(Error::BAD_ARGUMENT);
    }
    if (is_failed()) {
        return std::unexpected(Error::DISABLED);
    }

    for (uint8_t i = 0U; i < AS7265X_CHANNEL_COUNT; i++) {
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

        result->channels[i] = (static_cast<uint16_t>(*hi) << 8U) | static_cast<uint16_t>(*lo);
    }

    clear_failures();
    return {};
}

Result<void> AS7265X::wait_tx_ready() {
    const uint32_t deadline = (tick != nullptr) ? (tick() + TIMEOUT_MS) : 0U;
    for (uint32_t i = 0U; i < BUSY_ITER; i++) {
        auto status = bus->read_reg8(addr, REG_STATUS);
        if (!status) {
            return std::unexpected(status.error());
        }
        if ((*status & STATUS_TX_FULL) == 0U) {
            return {};
        }
        if (tick != nullptr && tick() >= deadline) {
            return std::unexpected(Error::TIMEOUT);
        }
    }
    return std::unexpected(Error::TIMEOUT);
}

Result<void> AS7265X::wait_rx_ready() {
    const uint32_t deadline = (tick != nullptr) ? (tick() + TIMEOUT_MS) : 0U;
    for (uint32_t i = 0U; i < BUSY_ITER; i++) {
        auto status = bus->read_reg8(addr, REG_STATUS);
        if (!status) {
            return std::unexpected(status.error());
        }
        if ((*status & STATUS_RX_VALID) != 0U) {
            return {};
        }
        if (tick != nullptr && tick() >= deadline) {
            return std::unexpected(Error::TIMEOUT);
        }
    }
    return std::unexpected(Error::TIMEOUT);
}

Result<void> AS7265X::write_virtual(uint8_t vreg, uint8_t value) {
    if (auto r = wait_tx_ready(); !r) {
        return r;
    }
    if (auto r = bus->write_reg8(addr, REG_WRITE, static_cast<uint8_t>(vreg | VWRITE_FLAG)); !r) {
        return r;
    }
    if (auto r = wait_tx_ready(); !r) {
        return r;
    }
    return bus->write_reg8(addr, REG_WRITE, value);
}

Result<uint8_t> AS7265X::read_virtual(uint8_t vreg) {
    if (auto r = wait_tx_ready(); !r) {
        return std::unexpected(r.error());
    }
    if (auto r = bus->write_reg8(addr, REG_WRITE, vreg); !r) {
        return std::unexpected(r.error());
    }
    if (auto r = wait_rx_ready(); !r) {
        return std::unexpected(r.error());
    }
    return bus->read_reg8(addr, REG_READ);
}
