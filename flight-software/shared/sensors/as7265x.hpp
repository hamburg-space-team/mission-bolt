#pragma once

#include "cmsis_i2c_bus.hpp"
#include "device_base.hpp"
#include "errors.hpp"

#include <cstdint>

constexpr uint8_t AS7265X_ADDR = 0x49U;
constexpr uint8_t AS7265X_CHANNEL_COUNT = 18U;

// Integration time: IT * 2.8ms
constexpr uint8_t AS7265X_INT_25_CYCLES = 40U;

/// @ingroup sensors
enum class AS7265XGain : uint8_t {
    GAIN_1X = 0x00U,
    GAIN_37X = 0x01U,
    GAIN_16X = 0x02U,
    GAIN_64X = 0x03U,
};

/// One full 18-channel result block, raw register values.
///
/// @ingroup sensors
struct AS7265XResult {
    uint16_t channels[AS7265X_CHANNEL_COUNT]; // NOLINT(modernize-avoid-c-arrays)
};

/// AMS AS7265X 18-channel multispectral sensor. The chip exposes a
/// small hardware register window and a larger virtual register space
/// reached through a state-machine; this driver hides that behind
/// write_virtual / read_virtual. Polled or interrupt-driven completion.
///
/// @ingroup sensors
class AS7265X : public DeviceBase {
  public:
    using tick_fn = uint32_t (*)();
    using delay_fn = void (*)(uint32_t ms);

    // data_rdy_fn: returns true when the hardware INT pin signals
    // DATA_READY (active-low - caller inverts). Pass nullptr to fall
    // back to polling VREG_CONTROL via I2C.
    using data_rdy_fn = bool (*)();

    /// tick: millisecond tick for virtual-register timeout.
    /// delay: millisecond delay used BETWEEN status polls (SparkFun
    /// reference: continuous polling starves the AS72651 firmware and the
    /// mailbox never turns ready). nullptr = tight polling (host tests).
    [[nodiscard]] Result<void> init(CmsisI2CBus* bus, tick_fn tick, uint8_t addr = AS7265X_ADDR,
                                    uint8_t integration_cycles = AS7265X_INT_25_CYCLES,
                                    AS7265XGain gain = AS7265XGain::GAIN_1X, data_rdy_fn int_pin = nullptr,
                                    delay_fn delay = nullptr);

    /// Boot probe
    [[nodiscard]] Result<void> probe(CmsisI2CBus* bus, tick_fn tick, delay_fn delay);

    /// Trigger a one-shot measurement. Returns immediately; record the
    /// start timestamp right after.
    [[nodiscard]] Result<void> start_measurement();

    /// Change the integration time (cycles x 2.8 ms) for subsequent
    /// measurements. No-op when the value is already set, so calling it
    /// every measurement is cheap.
    [[nodiscard]] Result<void> set_integration(uint8_t cycles);

    /// True when the most recent measurement has completed.
    [[nodiscard]] bool data_ready();

    /// Read all 18 channels into the caller-owned result. Only call
    /// when data_ready() == true. AS7265XResult is 36 bytes; we keep
    /// the out-param to avoid the implicit copy that the
    /// expected<AS7265XResult, Error> return would force.
    [[nodiscard]] Result<void> read_channels(AS7265XResult* result);

    /// Read only the dies [first_die, first_die+die_count) (6 channels
    /// each) into their fixed slots of result->channels. Lets the caller
    /// split the full read across ticks. NOTE: the RAW registers are LIVE
    /// - starting the next measurement resets them - so every read must
    /// happen after DATA_RDY and before the next start_measurement().
    [[nodiscard]] Result<void> read_channels_dies(AS7265XResult* result, uint8_t first_die, uint8_t die_count);

  private:
    static constexpr uint8_t VREG_HW_VERSION = 0x01U;
    static constexpr uint8_t VREG_CONTROL = 0x04U;
    static constexpr uint8_t VREG_INT_TIME = 0x05U;
    static constexpr uint8_t REG_STATUS = 0x00U;
    static constexpr uint8_t REG_READ = 0x02U;
    static constexpr uint8_t STATUS_TX_FULL = 0x02U;
    static constexpr uint8_t STATUS_RX_VALID = 0x01U;
    static constexpr uint8_t DATA_RDY_BIT = 0x02U;

    static constexpr uint8_t INT_ENABLE_BIT = 0x40U;
    static constexpr uint8_t VWRITE_FLAG = 0x80U;
    static constexpr uint8_t MODE_ONE_SHOT = 0b11U;
    static constexpr uint32_t TIMEOUT_MS = 20U;
    static constexpr uint32_t BUSY_ITER = 100U;

    static constexpr uint32_t POLLING_DELAY_MS = 1U;

    static constexpr uint8_t FIRST_CHANNEL_VREG = 0x08U;
    static constexpr uint8_t VREG_DEV_SEL = 0x4FU;
    static constexpr uint8_t DEVICE_COUNT = 3U;
    static constexpr uint8_t CHANNELS_PER_DEVICE = 6U;

    [[nodiscard]] Result<void> write_virtual(uint8_t vreg, uint8_t value);
    [[nodiscard]] Result<uint8_t> read_virtual(uint8_t vreg);
    [[nodiscard]] Result<void> wait_status(uint8_t mask, uint8_t want);
    [[nodiscard]] Result<void> wait_tx_ready();
    [[nodiscard]] Result<void> wait_rx_ready();

    [[nodiscard]] Result<uint8_t> hw_read_reg(uint8_t reg);

    CmsisI2CBus* bus = nullptr;
    tick_fn tick = nullptr;
    delay_fn delay_ms = nullptr;
    data_rdy_fn data_rdy_pin = nullptr;
    uint8_t addr = 0U;
    uint8_t current_cycles = 0U; // last integration setting written to the chip
};
