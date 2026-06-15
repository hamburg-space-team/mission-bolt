#include "node_computer.hpp"

NodeComputer::NodeComputer(const Platform& platform, CmsisI2CBus& i2c, Store& storage) noexcept
    : FlightComputer(platform), i2c(i2c), storage(storage), leds(Led{platform.led_can}, Led{platform.led_err}),
      boot(BootState::read()) {
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

void NodeComputer::on_drain(uint32_t deadline_ms) {
    // ADR-004: drain queued SD writes while the remaining tick budget
    // exceeds MIN_TIME_FOR_WRITE_MS. We stop early on either the
    // budget running out or the ring buffer going empty. If an
    // individual lfs_file_write() stalls beyond the budget the loop
    // exits at the top of the next iteration; the missed tick is
    // counted at the next on_tick() call.
    while (true) {
        const uint32_t now = platform.tick_ms();
        if ((now + MIN_TIME_FOR_WRITE_MS) >= deadline_ms) {
            break;
        }
        if (!storage.drain_one()) {
            break; // ring buffer empty
        }
    }
}
