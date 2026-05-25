#include "node_computer.hpp"

NodeComputer::NodeComputer(const Platform& platform, CmsisI2CBus& i2c) noexcept
    : FlightComputer(platform), i2c(i2c), leds(Led{platform.led_can}, Led{platform.led_err}), boot(BootState::read()) {
}

void NodeComputer::init_sensors() {
    (void)i2c.init();

    if (baro.init(&i2c, MS5611_ADDR, MS5611Osr::OSR_4096, platform.delay_ms) < 0) {
        baro.disable();
        leds.error_set();
        on_sensor_failed();
    }
    if (tmp.init(&i2c, TMP117_ADDR) < 0) {
        tmp.disable();
        leds.error_set();
        on_sensor_failed();
    }

    init_extra_sensors();
}

void NodeComputer::init_sd(void* hsd) {
    if (sd.init(hsd) < 0) {
        leds.error_set();
    }
}
