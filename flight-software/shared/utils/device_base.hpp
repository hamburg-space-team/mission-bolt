#pragma once

#include <cstdint>

/// @defgroup utils Utilities

/// Common fault-management state for hardware drivers.
///
/// Inherit and call register_failure() on a failed bus transaction.
/// After MAX_FAILURES consecutive failures the driver latches as
/// failed and further calls should short-circuit. Failures are never
/// cleared automatically in flight - see ADR-005.
///
/// @ingroup utils
class DeviceBase {
  public:
    /// True once the driver has been latched as failed.
    [[nodiscard]] bool is_failed() const;

    /// Force the failed state immediately. Used by init() paths that
    /// detect an unrecoverable problem before any failure counter has
    /// been incremented (WHO_AM_I mismatch, PROM CRC fail, ...).
    void disable();

  protected:
    static constexpr uint8_t MAX_FAILURES = 3U;

    void register_failure();
    void clear_failures();

  private:
    bool failed = false;
    uint8_t fail_count = 0U;
};
