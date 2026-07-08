#include "node_computer.hpp"

NodeComputer::NodeComputer(const Platform& platform, CmsisI2CBus& i2c, Store& storage) noexcept
    : FlightComputer(platform), i2c(i2c), storage(storage), leds(Led{platform.led_can}, Led{platform.led_err}),
      boot(BootState::read()) {
}

void NodeComputer::init_sensors() {
    if (auto r = this->i2c.init(); !r) {
        report_fault(StatusLeds::Fault::I2C_BUS, r.error());
        return;
    }

    if (auto r = this->baro.init(&this->i2c, MS5611_ADDR, MS5611Osr::OSR_4096, this->platform.delay_ms); !r) {
        this->baro.disable(r.error());
        this->baro_fault_reported = true; // reported here, not by the tick poll
        report_fault(StatusLeds::Fault::BARO, r.error());
    }

    if (auto r = this->tmp.init(&this->i2c, TMP117_ADDR); !r) {
        this->tmp.disable(r.error());
        this->tmp_fault_reported = true;
        report_fault(StatusLeds::Fault::TMP, r.error());
    }

    init_extra_sensors();
}

void NodeComputer::init_storage() {
    if (auto r = this->storage.init(); !r) {
        // LED only, deliberately no FAULT packet: neither CAN nor the
        // downlink are ready this early, and the SD itself (the failed
        // part) could not log it. Ground sees sd_status in the status
        // packets instead; the trace stays readable via the debugger.
        (void)r.error();
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
