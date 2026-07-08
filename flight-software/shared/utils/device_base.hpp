#pragma once

#include "errors.hpp"

#include <cstdint>

/// @defgroup utils Utilities

/// Common fault-management state for hardware drivers.
///
/// Inherit and call register_failure() on a failed bus transaction.
/// After MAX_FAILURES consecutive failures the driver latches as
/// failed and further calls should short-circuit. Failures are never
/// cleared automatically in flight - see ADR-005.
///
/// Both latch paths take the Error that caused them: the device keeps
/// it as its "death trace" (last_error()), so a fault that is only
/// polled and reported later - or inspected with the debugger - still
/// shows the original step chain, line and timestamp (ADR-012).
///
/// @ingroup utils
class DeviceBase {
  public:
    /// True once the driver has been latched as failed.
    [[nodiscard]] bool is_failed() const;

    /// Force the failed state immediately. Used by init() paths that
    /// detect an unrecoverable problem before any failure counter has
    /// been incremented (WHO_AM_I mismatch, PROM CRC fail, ...).
    void disable(const Error& e);

    /// The Error that most recently registered a failure (the one
    /// that latched the device, unless calls kept failing after the
    /// latch). Default-constructed while no failure was ever seen.
    [[nodiscard]] const Error& last_error() const;

  protected:
    static constexpr uint8_t MAX_FAILURES = 3U;

    void register_failure(const Error& e);
    void clear_failures();

  private:
    Error last_err{};
    bool failed = false;
    uint8_t fail_count = 0U;
};
