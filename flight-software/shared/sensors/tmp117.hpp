#pragma once

#include "cmsis_i2c_bus.hpp"
#include "device_base.hpp"
#include "errors.hpp"

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
    /// Verify device ID and switch to continuous conversion.
    [[nodiscard]] Result<void> init(CmsisI2CBus* bus, uint8_t addr = TMP117_ADDR);

    /// Read the raw temperature register. Failures latch via
    /// DeviceBase.
    [[nodiscard]] Result<int16_t> read();

    /// Raw device-ID register (self-test WHOAMI). Diagnostic only - does
    /// not count toward the failure latch
    [[nodiscard]] Result<uint16_t> read_device_id();

    static constexpr uint16_t DEV_ID_MASK = 0x0FFFU;
    static constexpr uint16_t DEV_ID_EXPECTED = 0x0117U;

  private:
    static constexpr uint8_t REG_TEMP = 0x00U;
    static constexpr uint8_t REG_CONFIG = 0x01U;
    static constexpr uint8_t REG_DEV_ID = 0x0FU;

    static constexpr uint16_t CONFIG_CONTINUOUS = 0x0000U;

    CmsisI2CBus* bus = nullptr;
    uint8_t addr = 0U;
};
