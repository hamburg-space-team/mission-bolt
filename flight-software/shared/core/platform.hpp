#pragma once

#include <cstdint>

/// @defgroup core Core framework

/// Board-supplied function pointers consumed by FlightComputer and
/// subclasses. One Platform per board, instantiated in app/main.cpp.
/// Keeping HAL access behind these pointers lets the host test build
/// link against the same shared code without the STM32 HAL.
///
/// @ingroup core
struct Platform {
    using delay_fn = void (*)(uint32_t ms);
    using tick_fn = uint32_t (*)();
    using kick_fn = void (*)();
    using led_fn = void (*)(bool on); // true = LED on, false = off

    delay_fn delay_ms;
    tick_fn tick_ms;
    kick_fn kick_wdg;
    tick_fn tick_us;
    led_fn led_can = nullptr; // CAN activity LED
    led_fn led_err = nullptr; // Error LED
};
