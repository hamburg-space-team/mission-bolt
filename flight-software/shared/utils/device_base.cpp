#include "device_base.hpp"

bool DeviceBase::is_failed() const {
    return this->failed;
}

void DeviceBase::disable() {
    this->failed = true;
}

void DeviceBase::register_failure() {
    if (++this->fail_count >= MAX_FAILURES) {
        this->failed = true;
    }
}

void DeviceBase::clear_failures() {
    this->fail_count = 0U;
}
