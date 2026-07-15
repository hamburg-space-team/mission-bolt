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
    this->failed_at_us = (ErrorClock::now_us != nullptr) ? ErrorClock::now_us() : 0U;
}

bool DeviceBase::retry_due() const {
    if (!this->failed || ErrorClock::now_us == nullptr) {
        return false;
    }
    // Unsigned wrap-safe: the us clock wraps ~every 71 min, >> the 30 s cooldown.
    return (ErrorClock::now_us() - this->failed_at_us) >= RETRY_COOLDOWN_US;
}

void DeviceBase::clear_failures() {
    this->fail_count = 0U;
}

void DeviceBase::clear_latch() {
    this->failed = false;
    this->fail_count = 0U;
}
