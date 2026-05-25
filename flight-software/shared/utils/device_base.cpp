#include "device_base.hpp"

bool DeviceBase::is_failed() const {
    return failed;
}

void DeviceBase::disable() {
    failed = true;
}

void DeviceBase::register_failure() {
    if (++fail_count >= MAX_FAILURES) {
        failed = true;
    }
}

void DeviceBase::clear_failures() {
    fail_count = 0U;
}