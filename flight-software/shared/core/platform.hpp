#pragma once

#include <cstdint>

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
