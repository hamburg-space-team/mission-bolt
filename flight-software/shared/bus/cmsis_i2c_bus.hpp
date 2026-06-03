#pragma once

#include "Driver_I2C.h"

#include <cstddef>
#include <cstdint>

/// @defgroup bus Hardware buses

/// Wraps ARM_DRIVER_I2C with a register-oriented API. One instance per
/// physical I2C controller. Every transaction is bounded: by the
/// optional ms-tick when supplied, otherwise by a fixed-iteration spin
/// (I-2).
///
/// @ingroup bus
class CmsisI2CBus {
  public:
    using tick_fn = uint32_t (*)();

    /// tick = nullptr falls back to the iteration-count timeout.
    CmsisI2CBus(ARM_DRIVER_I2C* drv, tick_fn tick = nullptr);

    /// Initialize the underlying CMSIS driver, bring it to full power,
    /// configure for fast mode (400 kHz). Must be called once before
    /// any read/write. Returns 0 on success.
    [[nodiscard]] int init();

    [[nodiscard]] int write(uint8_t addr, const uint8_t* data, size_t len);
    [[nodiscard]] int read(uint8_t addr, uint8_t* data, size_t len);

    /// Write then read with repeated start.
    [[nodiscard]] int write_read(uint8_t addr, const uint8_t* tx, size_t tx_len, uint8_t* rx, size_t rx_len);

    [[nodiscard]] int write_reg8(uint8_t addr, uint8_t reg, uint8_t value);
    [[nodiscard]] int write_reg16(uint8_t addr, uint8_t reg, uint16_t value);
    [[nodiscard]] int read_reg8(uint8_t addr, uint8_t reg, uint8_t* value);
    [[nodiscard]] int read_reg16(uint8_t addr, uint8_t reg, uint16_t* value);

  private:
    static constexpr uint32_t BUSY_TIMEOUT = 100000U;
    static constexpr uint32_t I2C_TIMEOUT_MS = 2U;

    ARM_DRIVER_I2C* drv = nullptr;
    tick_fn get_tick = nullptr;

    [[nodiscard]] int wait_busy() const;
};
