#pragma once

#include "cmsis_i2c_bus.hpp"
#include "device_base.hpp"

#include <cstdint>

/// TI LP5810 4-channel constant-current LED driver.
///
/// I2C addressing: Addr = (chip_5bit << 2) | (reg >> 8). Pass the
/// page-0 base address to init() - all registers used here are page 0
/// (0x000-0x0FF).
///
/// LED control is purely via LED_EN (0x020): dot current and PWM are
/// pre-armed in init(). Call set_channels(mask) to illuminate;
/// disable_all() to extinguish.
///
/// Used by EXP1 to drive the spectrometer illumination LEDs; D-360
/// requires intensity to stay below detector saturation, controlled by
/// the dc parameter.
///
/// @ingroup led
class LP5810 : public DeviceBase {
  public:
    /// addr: page-0 I2C address (e.g. 0x14)
    /// dc:   per-channel dot current 0x00-0xFF, scales 0-25.5 mA
    ///       (max_current=0)
    [[nodiscard]] int init(CmsisI2CBus* bus, uint8_t addr, uint8_t dc = 0xFFU);

    /// Write mask (bits 3:0) to LED_EN - enables those channels,
    /// disables the rest.
    [[nodiscard]] int set_channels(uint8_t mask);

    /// Turn off all channels (LED_EN = 0).
    [[nodiscard]] int disable_all();

  private:
    static constexpr uint8_t REG_CHIP_EN = 0x00U;
    static constexpr uint8_t REG_CFG0 = 0x01U;   // bit 0: max_current (0=25.5mA, 1=51mA)
    static constexpr uint8_t REG_CFG12 = 0x0DU;  // lsd_threshold in bits [3:2]
    static constexpr uint8_t REG_UPDATE = 0x10U; // write CMD_UPDATE to apply CONFIG regs
    static constexpr uint8_t REG_LED_EN = 0x20U; // bits [3:0] enable channels 0-3
    static constexpr uint8_t REG_RESET = 0x23U;  // write CMD_RESET for soft reset
    static constexpr uint8_t REG_DC0 = 0x30U;    // DC0-DC3 at 0x30-0x33
    static constexpr uint8_t REG_PWM0 = 0x40U;   // PWM0-PWM3 at 0x40-0x43
    static constexpr uint8_t CMD_UPDATE = 0x55U;
    static constexpr uint8_t CMD_RESET = 0xFFU;
    static constexpr uint8_t CHANNEL_COUNT = 4U;

    [[nodiscard]] int write_reg(uint8_t reg, uint8_t value);

    CmsisI2CBus* bus = nullptr;
    uint8_t addr = 0U;
};
