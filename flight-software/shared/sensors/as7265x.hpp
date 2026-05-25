#pragma once

#include "cmsis_i2c_bus.hpp"
#include "device_base.hpp"

#include <cstdint>

constexpr uint8_t AS7265X_ADDR = 0x49U;
constexpr uint8_t AS7265X_CHANNEL_COUNT = 18U;

// Integration time: IT * 2.8ms
constexpr uint8_t AS7265X_INT_25_CYCLES = 25U;

enum class AS7265XGain : uint8_t {
    GAIN_1X = 0x00U,
    GAIN_37X = 0x01U,
    GAIN_16X = 0x02U,
    GAIN_64X = 0x03U,
};

struct AS7265XResult {
    uint16_t channels[AS7265X_CHANNEL_COUNT]; // NOLINT(modernize-avoid-c-arrays)
};

// AS7265X 18-channel multispectral sensor driver.:
class AS7265X : public DeviceBase {
  public:
    using tick_fn = uint32_t (*)();
    // data_rdy_fn: returns true when the hardware INT pin signals DATA_READY (active-low - caller inverts).
    // Pass nullptr to fall back to polling VREG_CONTROL via I2C.
    using data_rdy_fn = bool (*)();

    // tick: millisecond tick for virtual-register timeout.
    [[nodiscard]] int init(CmsisI2CBus* bus, tick_fn tick, uint8_t addr = AS7265X_ADDR,
                           uint8_t integration_cycles = AS7265X_INT_25_CYCLES, AS7265XGain gain = AS7265XGain::GAIN_1X,
                           data_rdy_fn int_pin = nullptr);

    // Trigger a one-shot measurement. Returns immediately; record timestamp right after.
    [[nodiscard]] int start_measurement();

    // Returns true when measurement is complete.
    [[nodiscard]] bool data_ready();

    // Read all 18 channels. Call only when data_ready() == true.
    [[nodiscard]] int read_channels(AS7265XResult* result);

  private:
    static constexpr uint8_t VREG_CONTROL = 0x04U;
    static constexpr uint8_t VREG_INT_TIME = 0x05U;
    static constexpr uint8_t REG_STATUS = 0x00U;
    static constexpr uint8_t REG_READ = 0x02U;
    static constexpr uint8_t STATUS_TX_FULL = 0x02U;
    static constexpr uint8_t STATUS_RX_VALID = 0x01U;
    static constexpr uint8_t DATA_RDY_BIT = 0x02U;
    static constexpr uint8_t INT_ENABLE_BIT = 0x80U; // bit 7 of VREG_CONTROL
    static constexpr uint8_t VWRITE_FLAG = 0x80U;
    static constexpr uint8_t MODE_ONE_SHOT = 0b11U;
    static constexpr uint32_t TIMEOUT_MS = 10U;
    static constexpr uint32_t BUSY_ITER = 100U;
    static constexpr uint8_t FIRST_CHANNEL_VREG = 0x08U;

    [[nodiscard]] int write_virtual(uint8_t vreg, uint8_t value);
    [[nodiscard]] int read_virtual(uint8_t vreg, uint8_t* value);
    [[nodiscard]] int wait_tx_ready();
    [[nodiscard]] int wait_rx_ready();

    CmsisI2CBus* bus = nullptr;
    tick_fn tick = nullptr;
    data_rdy_fn data_rdy_pin = nullptr;
    uint8_t addr = 0U;
};
