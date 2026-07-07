#pragma once

#include "Driver_I2C.h"
#include "errors.hpp"

#include <cstddef>
#include <cstdint>

/// @defgroup bus Hardware buses

/// Wraps ARM_DRIVER_I2C with a register-oriented API.
///
/// @ingroup bus
class CmsisI2CBus {
  public:
    using tick_fn = uint32_t (*)();

    /// tick = nullptr falls back to the iteration-count timeout.
    CmsisI2CBus(ARM_DRIVER_I2C* drv, tick_fn tick = nullptr);

    [[nodiscard]] Result<void> init();

    /// Full peripheral reset: powers the CMSIS driver off (HAL_I2C_DeInit,
    /// which releases SCL/SDA and clears any latched BERR / arbitration-lost /
    /// lock-up), then re-runs init().
    [[nodiscard]] Result<void> reset();

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

    /// SignalEvent callback registered with the CMSIS driver. The callback
    /// type carries no user context, so the latched event is necessarily
    /// shared static state
    static void signal_event(uint32_t event);
    static volatile uint32_t last_event;

    /// Block until the SignalEvent callback reports the transaction finished,
    /// then map the event bitmask to a Result.
    [[nodiscard]] Result<void> wait_complete() const;
};