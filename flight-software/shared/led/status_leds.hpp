#pragma once

#include "led.hpp"

/// Board-level status LED pair.
///
/// CAN LED  - blinks on every tick that carries valid CAN activity.
///            Stops blinking if the CAN bus breaks (no more frames arrive).
/// Error LED - turns on and stays on when any non-recoverable fault occurs
///             (sensor init failure, SD mount failure, ...).
///
/// @ingroup led
class StatusLeds {
  public:
    constexpr StatusLeds() noexcept = default;
    constexpr StatusLeds(Led can_led, Led err_led) noexcept : can(can_led), err(err_led) {
    }

    /// Call once per tick when CAN is healthy (SYNC received / EXP
    /// frame drained).
    void can_tick() noexcept {
        can.toggle();
    }

    /// Call when a tick passes with no CAN activity - extinguishes the LED.
    void can_lost() noexcept {
        can.off();
    }

    /// Latch the error LED on. Intentionally non-clearable in flight:
    /// once a fault occurs the LED stays on until the next reboot.
    void error_set() noexcept {
        err.on();
    }

  private:
    Led can;
    Led err;
};
