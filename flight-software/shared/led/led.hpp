#pragma once

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
        if (this->fn != nullptr) {
            this->fn(true);
        }
    }
    void off() const noexcept {
        if (this->fn != nullptr) {
            this->fn(false);
        }
    }
    /// Drive the LED to an explicit state; toggle() continues from it.
    void set(bool on) noexcept {
        this->state = on;
        if (this->fn != nullptr) {
            this->fn(this->state);
        }
    }
    void toggle() noexcept {
        this->state = !this->state;
        if (this->fn != nullptr) {
            this->fn(this->state);
        }
    }

  private:
    WriteFn fn = nullptr;
    bool state = false;
};
