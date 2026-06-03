#include "node_computer.hpp"

NodeComputer::NodeComputer(const Platform& platform, CmsisI2CBus& i2c, Store& storage) noexcept
    : FlightComputer(platform), i2c(i2c), storage(storage),
      leds(Led{platform.led_can}, Led{platform.led_err}), boot(BootState::read()) {
}

void NodeComputer::init_sensors() {
    if (!i2c.init()) {
        leds.error_set();
        on_sensor_failed();
        return;
    }

    if (!baro.init(&i2c, MS5611_ADDR, MS5611Osr::OSR_4096, platform.delay_ms)) {
        baro.disable();
        leds.error_set();
        on_sensor_failed();
    }
    if (!tmp.init(&i2c, TMP117_ADDR)) {
        tmp.disable();
        leds.error_set();
        on_sensor_failed();
    }

    init_extra_sensors();
}

void NodeComputer::init_storage() {
    if (!storage.init()) {
        leds.error_set();
    }
}
