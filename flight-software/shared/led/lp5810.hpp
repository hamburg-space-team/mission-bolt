#pragma once

#include "cmsis_i2c_bus.hpp"
#include "device_base.hpp"
#include "errors.hpp"

#include <cstdint>

/// TI LP5810 4-channel constant-current LED driver.
///
/// @ingroup led
class LP5810 : public DeviceBase {
  public:
    using delay_fn = void (*)(uint32_t ms);

    /// addr:  page-0 I2C address (e.g. 0x58 for the C variant)
    /// dc:    per-channel dot current 0x00-0xFF; full scale is 25.5 mA
    ///        (high_current=false) or 51 mA (high_current=true, sets
    ///        CFG0.max_current)
    /// delay: millisecond delay for the post-reset boot time; nullptr
    ///        skips the wait (host tests only)
    [[nodiscard]] Result<void> init(CmsisI2CBus* bus, uint8_t addr, uint8_t dc = 0xFFU, delay_fn delay = nullptr,
                                    bool high_current = false);

    /// Set all four channels' PWM duty (0xFF = 100 %), then write mask
    /// (bits 3:0) to LED_EN - enables those channels, disables the rest.
    [[nodiscard]] Result<void> set_channels(uint8_t mask, uint8_t pwm = 0xFFU);

    /// Turn off all channels (LED_EN = 0).
    [[nodiscard]] Result<void> disable_all();

    /// Presence probe: a bare LED_EN = 0 write. The part is write-only
    /// (ICD-005), so an ACK is all it offers. Bypasses the failed latch
    [[nodiscard]] Result<void> probe();

    [[nodiscard]] Result<void> reconfigure();

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
    static constexpr uint8_t CMD_RESET = 0x66U; // TRM 2.7.1: "Write 66h to reset"
    static constexpr uint8_t CHANNEL_COUNT = 4U;

    // Boot time after soft reset before the device accepts I2C again;
    // datasheet 9.2.3.3 quotes "around 1 ms" for the analogous power-up.
    // The chip can NACK (or silently drop) writes while still rebooting, so
    // enable_chip() re-attempts the Chip_EN write a few times after the delay.
    static constexpr uint32_t RESET_BOOT_DELAY_MS = 1U;
    static constexpr uint8_t CHIP_EN_WRITE_RETRIES = 8U;
    static constexpr uint32_t CHIP_EN_RETRY_DELAY_MS = 2U;

    [[nodiscard]] Result<void> write_reg(uint8_t reg, uint8_t value);

    /// Write Chip_EN=1 (moves the device INITIAL -> STANDBY), retrying the
    /// write while the chip is still rebooting from the soft reset. Write-only:
    /// an ACK is the presence check; there is no readback (ICD-005).
    [[nodiscard]] Result<void> enable_chip();

    /// Full power-on register sequence (optional reset, enable, config,
    /// pre-arm channels). Shared by init(), recover() and reconfigure().
    [[nodiscard]] Result<void> configure(bool reset_first);

    /// Full recovery for a failed runtime write: complete I2C bus reset, full
    /// chip re-init via configure(), then re-apply (reg,value). Clears the
    /// strike count on success; registers a failure (latching the device off
    /// after MAX_FAILURES consecutive recoveries) on failure.
    [[nodiscard]] Result<void> recover(uint8_t reg, uint8_t value);

    /// Plain write sequence for set_channels (PWM0-3, then LED_EN); returns
    /// the first failure so set_channels can run the recovery path and
    /// re-apply the WHOLE step afterwards.
    [[nodiscard]] Result<void> apply_channels(uint8_t mask, uint8_t pwm);

    CmsisI2CBus* bus = nullptr;
    uint8_t addr = 0U;
    uint8_t dot_current = 0xFFU; // per-channel dot current, retained for re-init
    bool max_current = false;    // CFG0.max_current: false=25.5mA, true=51mA full scale
    delay_fn delay_ms = nullptr; // post-reset boot delay, retained for re-init
};
