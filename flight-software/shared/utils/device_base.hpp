#pragma once

#include <cstdint>

class DeviceBase {
  public:
    [[nodiscard]] bool is_failed() const;

    void disable();

  protected:
    static constexpr uint8_t MAX_FAILURES = 3U;

    void register_failure();
    void clear_failures();

  private:
    bool failed = false;
    uint8_t fail_count = 0U;
};