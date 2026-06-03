#pragma once

#include "Driver_I2C.h"
#include "errors.hpp"

#include <cstddef>
#include <cstdint>

/// @defgroup bus Hardware buses

/// Wraps ARM_DRIVER_I2C with a register-oriented API. One instance per
/// physical I2C controller. Every transaction is bounded: by the
/// optional ms-tick when supplied, otherwise by a fixed-iteration spin
/// (I-2). All fallible calls return Result<void> or Result<value>; the
/// error code carries the cause back to the caller.
///
/// @ingroup bus
class CmsisI2CBus {
  public:
    using tick_fn = uint32_t (*)();

    /// tick = nullptr falls back to the iteration-count timeout.
    CmsisI2CBus(ARM_DRIVER_I2C* drv, tick_fn tick = nullptr);

    /// Initialize the underlying CMSIS driver, bring it to full power,
    /// configure for fast mode (400 kHz). Must be called once before
    /// any read/write.
    [[nodiscard]] Result<void> init();

    [[nodiscard]] Result<void> write(uint8_t addr, const uint8_t* data, std::size_t len);
    [[nodiscard]] Result<void> read(uint8_t addr, uint8_t* data, std::size_t len);

    /// Write then read with repeated start.
    [[nodiscard]] Result<void> write_read(uint8_t addr, const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                                          std::size_t rx_len);

    [[nodiscard]] Result<void> write_reg8(uint8_t addr, uint8_t reg, uint8_t value);
    [[nodiscard]] Result<void> write_reg16(uint8_t addr, uint8_t reg, uint16_t value);
    [[nodiscard]] Result<uint8_t> read_reg8(uint8_t addr, uint8_t reg);
    [[nodiscard]] Result<uint16_t> read_reg16(uint8_t addr, uint8_t reg);

  private:
    static constexpr uint32_t BUSY_TIMEOUT = 100000U;
    static constexpr uint32_t I2C_TIMEOUT_MS = 2U;

    ARM_DRIVER_I2C* drv = nullptr;
    tick_fn get_tick = nullptr;

    [[nodiscard]] Result<void> wait_busy() const;
};
