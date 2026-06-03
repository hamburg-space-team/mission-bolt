#pragma once

#include "cmsis_i2c_bus.hpp"
#include "device_base.hpp"

#include <cstdint>

constexpr uint8_t TMP117_ADDR = 0x48U;

/// TI TMP117 temperature sensor, continuous conversion mode. After
/// init() the sensor delivers a fresh sample every ~1s at 64-cycle
/// averaging. Raw register values are forwarded to ground; 1 LSB =
/// 1/128 degC, 16-bit signed.
///
/// @ingroup sensors
class TMP117 : public DeviceBase {
  public:
    /// Verify device ID and switch to continuous conversion. Returns 0
    /// on success, -1 on bus error or ID mismatch.
    [[nodiscard]] int init(CmsisI2CBus* bus, uint8_t addr = TMP117_ADDR);

    /// Read the raw temperature register into *raw. Failures latch via
    /// DeviceBase.
    [[nodiscard]] int read(int16_t* raw);

  private:
    static constexpr uint8_t REG_TEMP = 0x00U;
    static constexpr uint8_t REG_CONFIG = 0x01U;
    static constexpr uint8_t REG_DEV_ID = 0x0FU;

    static constexpr uint16_t DEV_ID_MASK = 0x0FFFU;
    static constexpr uint16_t DEV_ID_EXPECTED = 0x0117U;

    static constexpr uint16_t CONFIG_CONTINUOUS = 0x0000U;

    CmsisI2CBus* bus = nullptr;
    uint8_t addr = 0U;
};
