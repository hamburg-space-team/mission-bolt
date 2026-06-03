#pragma once

#include <cstdint>

/// @defgroup led LED drivers

/// Single-LED abstraction. WriteFn(true) = on, WriteFn(false) = off.
/// Default-constructed Led is a no-op (nullptr write function).
///
/// @ingroup led
class Led {
  public:
    using WriteFn = void (*)(bool on);

    constexpr Led() noexcept = default;
    constexpr explicit Led(WriteFn fn) noexcept : fn(fn) {
    }

    void on() const noexcept {
        if (fn != nullptr) {
            fn(true);
        }
    }
    void off() const noexcept {
        if (fn != nullptr) {
            fn(false);
        }
    }
    void toggle() noexcept {
        state = !state;
        if (fn != nullptr) {
            fn(true);
        }
    }

  private:
    WriteFn fn = nullptr;
    bool state = false;
};
