#pragma once

#include "errors.hpp"

#include <cstdint>

/// @defgroup utils Utilities

/// Common fault-management state for hardware drivers.
///
/// @ingroup utils
class DeviceBase {
  public:
    /// True once the driver has been latched as failed.
    [[nodiscard]] bool is_failed() const;

    /// Force the failed state immediately
    void disable(const Error& e);

    [[nodiscard]] const Error& last_error() const;

    /// Retry gate for a latched device
    [[nodiscard]] bool retry_due() const;
    void arm_retry();

    /// Clear the failed latch after a successful in-place recovery (re-init)
    void clear_latch();

  protected:
    static constexpr uint8_t MAX_FAILURES = 3U;
    static constexpr uint32_t RETRY_COOLDOWN_US = 30U * 1000U * 1000U; // 30 s deactivation before a retry

    void register_failure(const Error& e);
    void clear_failures();

  private:
    Error last_err{};
    bool failed = false;
    uint8_t fail_count = 0U;
    uint32_t failed_at_cyc = 0U; // raw cycles at latch; start of the retry cooldown
};
