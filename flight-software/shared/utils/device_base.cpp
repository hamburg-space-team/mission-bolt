#include "device_base.hpp"

bool DeviceBase::is_failed() const {
    return this->failed;
}

void DeviceBase::disable(const Error& e) {
    this->last_err = e;
    this->failed = true;
    arm_retry();
}

const Error& DeviceBase::last_error() const {
    return this->last_err;
}

void DeviceBase::register_failure(const Error& e) {
    this->last_err = e;
    if (++this->fail_count >= MAX_FAILURES) {
        this->failed = true;
        arm_retry();
    }
}

void DeviceBase::arm_retry() {
    this->failed_at_cyc = (ErrorClock::now_cycles != nullptr) ? ErrorClock::now_cycles() : 0U;
}

bool DeviceBase::retry_due() const {
    if (!this->failed || ErrorClock::now_cycles == nullptr) {
        return false;
    }
    // cycle domain: the us clock wraps every 53.7 s (< 2x the cooldown), so
    // us differences would fire early on more than half of all cooldowns
    return ErrorClock::us_between(this->failed_at_cyc, ErrorClock::now_cycles()) >= RETRY_COOLDOWN_US;
}

void DeviceBase::clear_failures() {
    this->fail_count = 0U;
}

void DeviceBase::clear_latch() {
    this->failed = false;
    this->fail_count = 0U;
}
