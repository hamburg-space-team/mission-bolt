#include "as7265x.hpp"

static constexpr uint8_t REG_WRITE = 0x01U;

int AS7265X::init(CmsisI2CBus* bus_in, tick_fn tick_in, uint8_t addr_in, uint8_t integration_cycles, AS7265XGain gain,
                  data_rdy_fn int_pin) {
    bus = bus_in;
    tick = tick_in;
    addr = addr_in;
    data_rdy_pin = int_pin;

    int ret = write_virtual(VREG_INT_TIME, integration_cycles);
    if (ret < 0) {
        disable();
        return ret;
    }

    // Gain at bits [5:4]; enable hardware INT if pin provided; leave BANK=0 (no measurement yet).
    auto ctrl = static_cast<uint8_t>(static_cast<uint8_t>(gain) << 4U);
    if (int_pin != nullptr) {
        ctrl = static_cast<uint8_t>(ctrl | INT_ENABLE_BIT);
    }
    ret = write_virtual(VREG_CONTROL, ctrl);
    if (ret < 0) {
        disable();
        return ret;
    }

    clear_failures();
    return 0;
}

int AS7265X::start_measurement() {
    if (is_failed()) {
        return -1;
    }

    uint8_t ctrl = 0U;
    int ret = read_virtual(VREG_CONTROL, &ctrl);
    if (ret < 0) {
        register_failure();
        return ret;
    }

    // Set BANK bits [3:2] to one-shot, preserve gain and INT bits.
    const auto new_ctrl = static_cast<uint8_t>((ctrl & 0xF3U) | static_cast<uint8_t>(MODE_ONE_SHOT << 2U));
    ret = write_virtual(VREG_CONTROL, new_ctrl);
    if (ret < 0) {
        register_failure();
    }
    return ret;
}

bool AS7265X::data_ready() {
    if (is_failed()) {
        return false;
    }
    if (data_rdy_pin != nullptr) {
        return data_rdy_pin();
    }
    uint8_t ctrl = 0U;
    if (read_virtual(VREG_CONTROL, &ctrl) < 0) {
        return false;
    }
    return (ctrl & DATA_RDY_BIT) != 0U;
}

int AS7265X::read_channels(AS7265XResult* result) {
    if (result == nullptr || is_failed()) {
        return -1;
    }

    for (uint8_t i = 0U; i < AS7265X_CHANNEL_COUNT; i++) {
        uint8_t hi = 0U;
        uint8_t lo = 0U;
        const auto vreg_hi = static_cast<uint8_t>(FIRST_CHANNEL_VREG + i * 2U);
        const auto vreg_lo = static_cast<uint8_t>(FIRST_CHANNEL_VREG + i * 2U + 1U);

        if (read_virtual(vreg_hi, &hi) < 0 || read_virtual(vreg_lo, &lo) < 0) {
            register_failure();
            return -1;
        }

        result->channels[i] = (static_cast<uint16_t>(hi) << 8U) | static_cast<uint16_t>(lo);
    }

    clear_failures();
    return 0;
}

int AS7265X::wait_tx_ready() {
    const uint32_t deadline = (tick != nullptr) ? (tick() + TIMEOUT_MS) : 0U;
    for (uint32_t i = 0U; i < BUSY_ITER; i++) {
        uint8_t status = 0U;
        if (bus->read_reg8(addr, REG_STATUS, &status) < 0) {
            return -1;
        }
        if ((status & STATUS_TX_FULL) == 0U) {
            return 0;
        }
        if (tick != nullptr && tick() >= deadline) {
            return -1;
        }
    }
    return -1;
}

int AS7265X::wait_rx_ready() {
    const uint32_t deadline = (tick != nullptr) ? (tick() + TIMEOUT_MS) : 0U;
    for (uint32_t i = 0U; i < BUSY_ITER; i++) {
        uint8_t status = 0U;
        if (bus->read_reg8(addr, REG_STATUS, &status) < 0) {
            return -1;
        }
        if ((status & STATUS_RX_VALID) != 0U) {
            return 0;
        }
        if (tick != nullptr && tick() >= deadline) {
            return -1;
        }
    }
    return -1;
}

int AS7265X::write_virtual(uint8_t vreg, uint8_t value) {
    int ret = wait_tx_ready();
    if (ret < 0) {
        return ret;
    }
    ret = bus->write_reg8(addr, REG_WRITE, static_cast<uint8_t>(vreg | VWRITE_FLAG));
    if (ret < 0) {
        return ret;
    }
    ret = wait_tx_ready();
    if (ret < 0) {
        return ret;
    }
    return bus->write_reg8(addr, REG_WRITE, value);
}

int AS7265X::read_virtual(uint8_t vreg, uint8_t* value) {
    int ret = wait_tx_ready();
    if (ret < 0) {
        return ret;
    }
    ret = bus->write_reg8(addr, REG_WRITE, vreg);
    if (ret < 0) {
        return ret;
    }
    ret = wait_rx_ready();
    if (ret < 0) {
        return ret;
    }
    return bus->read_reg8(addr, REG_READ, value);
}
