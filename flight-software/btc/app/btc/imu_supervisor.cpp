#include "imu_supervisor.hpp"

Result<void> ImuSupervisor::init() noexcept {
    if (auto r = imu.init(&i2c, ICM42686_ADDR, ICM42686Odr::ODR_200HZ, true); !r) {
        imu.disable(r.error());
        return r;
    }
    return {};
}

std::optional<Error> ImuSupervisor::retry() noexcept {
    if (!imu.retry_due()) {
        return std::nullopt;
    }

    platform.kick_wdg(); // WHO_AM_I retries can be a run of I2C timeouts on a sick bus

    ICM42686 fresh;
    if (fresh.init(&i2c, ICM42686_ADDR, ICM42686Odr::ODR_200HZ, true)) {
        imu = fresh;
        return std::nullopt;
    }

    imu.arm_retry();
    return imu.last_error();
}
