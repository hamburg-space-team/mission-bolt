#pragma once

#include "cmsis_i2c_bus.hpp"
#include "errors.hpp"
#include "icm42686.hpp"
#include "platform.hpp"

#include <optional>

/// Owns the BTC's ICM-42686 IMU together with its fault/retry policy
///
/// @ingroup apps
class ImuSupervisor {
  public:
    ImuSupervisor(const Platform& platform, CmsisI2CBus& i2c) noexcept : platform(platform), i2c(i2c) {
    }

    [[nodiscard]] Result<void> init() noexcept;
    [[nodiscard]] std::optional<Error> retry() noexcept;
    [[nodiscard]] auto read_sample() noexcept {
        return imu.read_sample();
    }

    // diagnostic surface for ImuSelfTest, same shape as the raw driver
    [[nodiscard]] auto read_who_am_i() noexcept {
        return imu.read_who_am_i();
    }
    [[nodiscard]] bool is_failed() const noexcept {
        return imu.is_failed();
    }

  private:
    const Platform& platform; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    CmsisI2CBus& i2c;         // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    ICM42686 imu{};
};
