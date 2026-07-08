#include "device_base.hpp"

bool DeviceBase::is_failed() const {
    return this->failed;
}

void DeviceBase::disable(const Error& e) {
    this->last_err = e;
    this->failed = true;
}

const Error& DeviceBase::last_error() const {
    return this->last_err;
}

void DeviceBase::register_failure(const Error& e) {
    this->last_err = e;
    if (++this->fail_count >= MAX_FAILURES) {
        this->failed = true;
    }
}

void DeviceBase::clear_failures() {
    this->fail_count = 0U;
}
