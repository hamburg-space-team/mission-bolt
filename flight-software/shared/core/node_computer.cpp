#include "node_computer.hpp"

NodeComputer::NodeComputer(const Platform& platform, CmsisI2CBus& i2c, Store& storage) noexcept
    : FlightComputer(platform), i2c(i2c), storage(storage), leds(Led{platform.led_can}, Led{platform.led_err}),
      boot(BootState::read()) {
}

void NodeComputer::init_sensors() {
    if (auto r = this->i2c.init(); !r) {
        this->i2c.disable(r.error());
        this->leds.set_fault(StatusLeds::Fault::I2C_BUS);
        return;
    }

    if (auto r = this->baro.init(&this->i2c, MS5611_ADDR, MS5611Osr::OSR_4096, this->platform.delay_ms); !r) {
        this->baro.disable(r.error());
        this->leds.set_fault(StatusLeds::Fault::BARO);
    }

    if (auto r = this->tmp.init(&this->i2c, TMP117_ADDR); !r) {
        this->tmp.disable(r.error());
        this->leds.set_fault(StatusLeds::Fault::TMP);
    }

    init_extra_sensors();
}

void NodeComputer::retry_failed_devices() {
    // Bus first: if it recovers, the sensors sharing it can re-init the same
    // tick. reset() = DeInit + init, which also clears a latched BERR /
    // arbitration-lost / lock-up.
    if (this->i2c.retry_due()) {
        this->platform.kick_wdg();

        if (this->i2c.reset()) {
            this->i2c.clear_latch();
        } else {
            this->i2c.arm_retry();
            report_fault(StatusLeds::Fault::I2C_BUS, this->i2c.last_error());
        }
    }

    if (this->baro.retry_due()) {
        this->platform.kick_wdg();

        MS5611 new_baro;
        if (new_baro.init(&this->i2c, MS5611_ADDR, MS5611Osr::OSR_4096, this->platform.delay_ms)) {
            this->baro = new_baro;
        } else {
            this->baro.arm_retry();
            report_fault(StatusLeds::Fault::BARO, this->baro.last_error());
        }
    }

    if (this->tmp.retry_due()) {
        this->platform.kick_wdg();

        TMP117 new_tmp;
        if (new_tmp.init(&this->i2c, TMP117_ADDR)) {
            this->tmp = new_tmp;
        } else {
            this->tmp.arm_retry();
            report_fault(StatusLeds::Fault::TMP, this->tmp.last_error());
        }
    }

    // The Store recovers in place (it owns the SDMMC handle + LittleFS buffers
    // and is held by reference, so it cannot be swapped for a fresh instance):
    // re-init re-mounts, clear_latch() lets writes resume.
    if (this->storage.retry_due()) {
        this->platform.kick_wdg();

        if (this->storage.init()) {
            this->storage.clear_latch();
        } else {
            this->storage.arm_retry();
            report_fault(StatusLeds::Fault::SD, this->storage.last_error());
        }
    }

    // Board-specific devices (IMU, ...) - each concrete node reuses the same
    // cooldown pattern.
    retry_extra_devices();
}

void NodeComputer::init_storage() {
    if (auto r = this->storage.init(); !r) {
        this->storage.disable(r.error());
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
            break;
        }
    }
}

// --- Common self-test steps ------------------------------------------------
// SKIPPED when the device is latched failed: the test observes, it does not
// re-poke hardware the recovery machinery owns

std::optional<PacketProtocol::TestResult> NodeComputer::step_tmp_whoami(NodeComputer& node, bool /*first*/,
                                                                        uint32_t& data) noexcept {
    if (node.tmp.is_failed()) {
        return PacketProtocol::TestResult::SKIPPED;
    }
    const auto id = node.tmp.read_device_id();
    if (!id) {
        return PacketProtocol::TestResult::FAIL;
    }
    data = *id; // full register, ground masks with DEV_ID_MASK
    return ((*id & TMP117::DEV_ID_MASK) == TMP117::DEV_ID_EXPECTED) ? PacketProtocol::TestResult::PASS
                                                                    : PacketProtocol::TestResult::FAIL;
}

std::optional<PacketProtocol::TestResult> NodeComputer::step_tmp_read(NodeComputer& node, bool /*first*/,
                                                                      uint32_t& data) noexcept {
    if (node.tmp.is_failed()) {
        return PacketProtocol::TestResult::SKIPPED;
    }
    // real flight read path; whether the value is sensible is ground's call
    const auto raw = node.tmp.read();
    if (!raw) {
        return PacketProtocol::TestResult::FAIL;
    }
    data = static_cast<uint16_t>(*raw);
    return PacketProtocol::TestResult::PASS;
}

std::optional<PacketProtocol::TestResult> NodeComputer::step_baro_prom(NodeComputer& node, bool /*first*/,
                                                                       uint32_t& data) noexcept {
    if (node.baro.is_failed()) {
        return PacketProtocol::TestResult::SKIPPED;
    }
    // NOT a baro.read() - a second read per tick harvests the env packet's
    // unfinished pipelined conversion. PROM+CRC is the chip's identity check
    const auto c1 = node.baro.verify_prom();
    if (!c1) {
        return PacketProtocol::TestResult::FAIL;
    }
    data = *c1; // C1 pressure sensitivity, a per-device fingerprint
    return PacketProtocol::TestResult::PASS;
}
