#include "node_computer.hpp"

NodeComputer::NodeComputer(const Platform& platform, CmsisI2CBus& i2c, Store& storage) noexcept
    : FlightComputer(platform), i2c(i2c), storage(storage), leds(Led{platform.led_can}, Led{platform.led_err}),
      boot(BootState::read()) {
}

void NodeComputer::init_sensors() {
    if (!this->i2c.init()) {
        on_sensor_failed(StatusLeds::Fault::I2C_BUS);
        return;
    }

    if (!this->baro.init(&this->i2c, MS5611_ADDR, MS5611Osr::OSR_4096, this->platform.delay_ms)) {
        this->baro.disable();
        on_sensor_failed(StatusLeds::Fault::BARO);
    }

    if (!this->tmp.init(&this->i2c, TMP117_ADDR)) {
        this->tmp.disable();
        on_sensor_failed(StatusLeds::Fault::TMP);
    }

    init_extra_sensors();
}

void NodeComputer::init_storage() {
    if (!this->storage.init()) {
        this->leds.set_fault(StatusLeds::Fault::SD);
    }
}

void NodeComputer::on_drain(uint32_t deadline_ms) {
    while (true) {
        const uint32_t now = this->platform.tick_ms();
        if ((now + MIN_TIME_FOR_WRITE_MS) >= deadline_ms) {
            break;
        }
        if (!this->storage.drain_one()) {
            break; // ring buffer empty
        }
    }
}
