#pragma once

#include <cstdint>

/// Tracks the REXUS Lift-Off (LO) signal
///
/// @ingroup apps
class LoTracker {
  public:
    template <typename RtcFn> bool service(bool edge, bool level_asserted, RtcFn&& rtc_now) noexcept {
        if (edge) {
            lo_received = true;
            lo_rtc_s = rtc_now();
            return true;
        }
        if (!lo_received && level_asserted) {
            if (++lo_level_ticks >= LO_LEVEL_DEBOUNCE_TICKS) {
                lo_received = true;
                lo_rtc_s = rtc_now();
                return true;
            }
        } else {
            lo_level_ticks = 0U;
        }
        return false;
    }

    [[nodiscard]] bool received() const noexcept {
        return lo_received;
    }

    /// RTC wall-clock second captured when LO latched (0 until then).
    [[nodiscard]] uint32_t rtc_s() const noexcept {
        return lo_rtc_s;
    }

  private:
    static constexpr uint8_t LO_LEVEL_DEBOUNCE_TICKS = 3U;

    bool lo_received = false;
    uint8_t lo_level_ticks = 0U;
    uint32_t lo_rtc_s = 0U;
};
