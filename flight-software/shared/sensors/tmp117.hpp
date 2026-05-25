#pragma once

#include "cmsis_i2c_bus.hpp"
#include "device_base.hpp"

#include <cstdint>

constexpr uint8_t TMP117_ADDR = 0x48U;

class TMP117 : public DeviceBase {
  public:
    [[nodiscard]] int init(CmsisI2CBus* bus, uint8_t addr = TMP117_ADDR);

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