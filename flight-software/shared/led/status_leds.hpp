#pragma once

#include <cstdint>

#include "led.hpp"

/// Board-level status LED pair.
///
/// CAN LED  - fast blink (toggles every 4th tick, ~3 Hz) while CAN SYNC is
///            healthy. Switches to a slow blink (~0.25 Hz) once the node
///            loses CAN and falls back to autonomous self-ticking.
///
/// Error LED - blinks a COUNTABLE PULSE CODE per fault source (see Fault).
///             Codes latch and are never cleared in flight (ADR-005). With
///             several faults latched the LED shows each code for
///             ERR_WINDOW_TICKS (10 s), then rotates to the next one.
///
/// Pattern of one code (all timing in 40 ms ticks via error_tick(); no
/// timers). Example code 3 (TMP117):
///
///   on   #####     #####     #####                            #####
///   off       #####     #####     ######################...###
///        | pulse  | pulse  | pulse  |----- code gap -----|  next repetition
///          8+8      8+8      8+8          40 ticks
///        (320 ms on / 320 ms off per pulse, 1.6 s between groups)
///
/// Full fault-code table (count the pulses): codes 1-5 exist on every
/// board, codes 6+ are board-specific. See docs/handbook/led-blink-codes.md.
///
///   1  I2C_BUS   I2C controller init failed        (all boards)
///   2  BARO      MS5611 failed                     (all boards)
///   3  TMP       TMP117 failed                     (all boards)
///   4  SD        storage mount/init failed         (all boards)
///   5  CAN       reserved: CAN bus fault           (all boards)
///   6  UART      RS-422 downlink dead              (BTC)
///   7  IMU       ICM-42686 failed                  (BTC, EXP3)
///   8  SPEC      AS7265X spectrometer failed       (EXP1)
///   9  LED_RGB   LP5810C RGB driver failed         (EXP1)
///  10  LED_UVIR  LP5810D UV/IR driver failed       (EXP1)
///
/// @ingroup led
class StatusLeds {
  public:
    /// Fault sources for the error-LED pulse codes. The numeric value IS
    /// the number of pulses blinked. Codes 1-5 are identical on every
    /// board (NodeComputer level); 6+ are board-specific.
    enum class Fault : uint8_t {
        I2C_BUS = 1,
        BARO = 2,
        TMP = 3,
        SD = 4,
        CAN_BUS = 5,
        UART = 6,
        IMU = 7,
        SPEC = 8,
        LED_RGB = 9,
        LED_UVIR = 10,
    };
    static constexpr uint8_t FAULT_COUNT = 10U;

    constexpr StatusLeds() noexcept = default;
    constexpr StatusLeds(Led can_led, Led err_led) noexcept : can(can_led), err(err_led) {
    }

    /// Call once per tick when CAN is healthy, passing the bus-wide CAN
    /// tick (BTC: the sync_count it broadcasts, EXPs: the received SYNC
    /// tick). The LED state derives from that shared counter, so the CAN
    /// LEDs of every node blink in phase
    void can_tick(uint16_t bus_tick) noexcept {
        this->lost_div = 0U;
        this->can.set(((bus_tick / TICK_BLINK_DIV) & 1U) != 0U);
    }

    /// Call once per tick while running autonomously (CAN SYNC lost). Blinks
    /// the CAN LED slowly so it is visually distinct from the healthy
    /// fast blink, signalling "alive but self-ticking".
    void can_lost() noexcept {
        if (++this->lost_div >= LOST_BLINK_DIV) {
            this->lost_div = 0U;
            this->can.toggle();
        }
    }

    /// Latch a fault source. Intentionally non-clearable in flight
    /// (ADR-005): once latched, the code stays in the error LED's
    /// rotation until the next reboot.
    void set_fault(Fault code) noexcept {
        this->fault_mask |= static_cast<uint16_t>(1U << (static_cast<uint8_t>(code) - 1U));
    }

    /// True if any fault is latched (telemetry/status convenience).
    [[nodiscard]] bool has_faults() const noexcept {
        return this->fault_mask != 0U;
    }

    /// Drive the error LED. Call exactly once per 40 ms tick, every tick
    /// (also while waiting for SYNC) - the pattern timing is pure tick
    /// counting. No latched fault -> LED off.
    void error_tick() noexcept {
        if (this->fault_mask == 0U) {
            this->err.off();
            return;
        }
        if (this->active_code == 0U) {
            this->active_code = next_fault(0U);
        }

        // One pattern cycle: N pulses (on+gap each) followed by the code gap.
        auto pattern_len =
            static_cast<uint16_t>((this->active_code * (PULSE_ON_TICKS + PULSE_GAP_TICKS)) + CODE_GAP_TICKS);
        if (this->pattern_ticks >= pattern_len) {
            this->pattern_ticks = 0U;
        }
        // Rotate to the next latched code only at a pattern boundary, so a
        // pulse group is never cut short mid-count.
        if (this->pattern_ticks == 0U && this->window_ticks >= ERR_WINDOW_TICKS) {
            this->active_code = next_fault(this->active_code);
            this->window_ticks = 0U;
            pattern_len =
                static_cast<uint16_t>((this->active_code * (PULSE_ON_TICKS + PULSE_GAP_TICKS)) + CODE_GAP_TICKS);
        }
        this->window_ticks++;

        const auto pulse_area = static_cast<uint16_t>(this->active_code * (PULSE_ON_TICKS + PULSE_GAP_TICKS));
        bool on = false;
        if (this->pattern_ticks < pulse_area) {
            on = (this->pattern_ticks % (PULSE_ON_TICKS + PULSE_GAP_TICKS)) < PULSE_ON_TICKS;
        }
        this->err.set(on);
        this->pattern_ticks++;
    }

  private:
    // Healthy blink: LED = (bus_tick / 4) & 1 -> 4 ticks on, 4 ticks off
    // (160 ms per phase at the 40 ms loop period, ~3 Hz). Derived from the
    // bus-wide tick, so all nodes blink in phase.
    static constexpr uint8_t TICK_BLINK_DIV = 4U;
    // Autonomous (CAN lost): 48 ticks x 40 ms loop period ~= 1.9 s per
    // toggle -> ~0.25 Hz. Deliberately far slower than the healthy blink
    // so the two states are unmistakable at a glance.
    static constexpr uint8_t LOST_BLINK_DIV = 48U;

    // Error-code pattern timing, all in 40 ms ticks.
    static constexpr uint16_t PULSE_ON_TICKS = 8U;  // 320 ms on
    static constexpr uint16_t PULSE_GAP_TICKS = 8U; // 320 ms off between pulses
    static constexpr uint16_t CODE_GAP_TICKS = 40U; // 1.6 s between repetitions
    static constexpr uint16_t ERR_WINDOW_TICKS = 250U;

    /// Next latched fault code strictly after `after` (wrapping); returns
    /// `after` itself when it is the only latched code, 0 when none is set.
    [[nodiscard]] uint8_t next_fault(uint8_t after) const noexcept {
        for (uint8_t i = 1U; i <= FAULT_COUNT; i++) {
            const auto code = static_cast<uint8_t>(((after + i - 1U) % FAULT_COUNT) + 1U);
            if ((this->fault_mask & (1U << (code - 1U))) != 0U) {
                return code;
            }
        }
        return 0U;
    }

    Led can;
    Led err;
    uint8_t lost_div = 0U;

    uint16_t fault_mask = 0U;    // bit (code-1) = fault latched
    uint8_t active_code = 0U;    // code currently being blinked (0 = none yet)
    uint16_t pattern_ticks = 0U; // position inside the current pattern cycle
    uint16_t window_ticks = 0U;  // ticks the active code has been shown
};
