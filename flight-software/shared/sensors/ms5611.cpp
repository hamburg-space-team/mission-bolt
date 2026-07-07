#include "ms5611.hpp"

Result<void> MS5611::init(CmsisI2CBus* bus, uint8_t addr, MS5611Osr osr, delay_fn delay) {
    this->bus = bus;
    this->addr = addr;
    this->osr = osr;
    this->delay_ms = delay;

    if (auto r = reset(); !r) {
        return r;
    }

    if (this->delay_ms != nullptr) {
        this->delay_ms(3U);
    }

    if (auto r = read_prom(); !r) {
        return r;
    }

    if (!prom_crc_ok()) {
        return std::unexpected(Error::PROTOCOL_ERROR);
    }

    // Restart the conversion pipeline: after a (re-)init the device has no
    // conversion running and old cached values must not leak into telemetry.
    this->pending = Pending::NONE;
    this->have_d1 = false;
    this->have_d2 = false;
    return {};
}

// ICD-005 pipelined read: collect the conversion started on the previous
// call, then start the next one (D2/D1 alternating). No blocking wait - the
// 40 ms tick period exceeds the worst-case conversion time (10 ms, OSR_4096),
// so the result is always ready by the time the next tick reads it.
Result<MS5611Result> MS5611::read() {
    if (is_failed()) {
        return std::unexpected(Error::DISABLED);
    }
    if (!this->coeff_read) {
        if (auto r = read_prom(); !r) {
            register_failure();
            return std::unexpected(r.error());
        }
    }

    // Collect the in-flight conversion (none on the first call after init).
    if (this->pending != Pending::NONE) {
        if (auto r = collect_pending(); !r) {
            this->pending = Pending::NONE; // device state unknown -> restart pipeline
            register_failure();
            return std::unexpected(r.error());
        }
    }

    // Start the next conversion: D1 after a completed D2, otherwise D2.
    const bool next_is_d1 = (this->pending == Pending::D2);
    const auto base_cmd = next_is_d1 ? CMD_CONVERT_D1 : CMD_CONVERT_D2;
    const auto cmd = static_cast<uint8_t>(base_cmd | (static_cast<uint8_t>(this->osr) * 2U));
    if (auto r = start_conversion(cmd); !r) {
        this->pending = Pending::NONE;
        register_failure();
        return std::unexpected(r.error());
    }
    this->pending = next_is_d1 ? Pending::D1 : Pending::D2;

    if (!this->have_d1 || !this->have_d2) {
        // Pipeline still priming (first two calls after init). Not a failed
        // transaction, so deliberately no DeviceBase strike (ICD-005).
        return std::unexpected(Error::TIMEOUT);
    }

    clear_failures();
    return MS5611Result{.d1 = this->d1_raw, .d2 = this->d2_raw};
}

Result<void> MS5611::reset() {
    return this->bus->write(this->addr, &CMD_RESET, 1U);
}

Result<void> MS5611::read_prom() {
    for (uint8_t i = 0U; i < COEFF_COUNT; i++) {
        const auto reg = static_cast<uint8_t>(PROM_BASE_ADDR + (i * 2U));

        std::array<uint8_t, 2> buf{};
        if (auto r = this->bus->write_read(this->addr, &reg, 1U, buf.data(), buf.size()); !r) {
            return r;
        }

        this->coeff[i] = static_cast<uint16_t>((static_cast<uint16_t>(buf[0]) << 8U) | static_cast<uint16_t>(buf[1]));
    }

    this->coeff_read = true;
    return {};
}

Result<void> MS5611::start_conversion(uint8_t cmd) {
    return this->bus->write(this->addr, &cmd, 1U);
}

// Read the ADC result of the in-flight conversion and store it into the
// d1/d2 slot selected by `pending`.
Result<void> MS5611::collect_pending() {
    const auto adc = read_adc_result();
    if (!adc) {
        return std::unexpected(adc.error());
    }
    if (*adc == 0U) {
        // Datasheet: the ADC reads 0 if the conversion was not finished -
        // means the >=1-conversion-time-per-tick assumption broke.
        return std::unexpected(Error::PROTOCOL_ERROR);
    }
    if (this->pending == Pending::D2) {
        this->d2_raw = *adc;
        this->have_d2 = true;
    } else {
        this->d1_raw = *adc;
        this->have_d1 = true;
    }
    return {};
}

Result<uint32_t> MS5611::read_adc_result() {
    if (auto r = this->bus->write(this->addr, &CMD_ADC_READ, 1U); !r) {
        return std::unexpected(r.error());
    }

    std::array<uint8_t, 3> buf{};
    if (auto r = this->bus->read(this->addr, buf.data(), sizeof(buf)); !r) {
        return std::unexpected(r.error());
    }

    return (static_cast<uint32_t>(buf[0]) << 16U) | (static_cast<uint32_t>(buf[1]) << 8U) |
           static_cast<uint32_t>(buf[2]);
}

bool MS5611::prom_crc_ok() const {
    std::array<uint16_t, COEFF_COUNT> prom = this->coeff;

    const auto crc_read = static_cast<uint8_t>(prom[IDX_CRC] & CRC_MASK);

    prom[IDX_CRC] &= 0xFF00U;

    uint32_t remainder = 0U;

    for (uint8_t cnt = 0U; cnt < (COEFF_COUNT * 2U); cnt++) {

        if ((cnt & 1U) == 1U) {
            remainder ^= static_cast<uint32_t>(prom[cnt >> 1U] & 0x00FFU);
        } else {
            remainder ^= static_cast<uint32_t>(prom[cnt >> 1U] >> 8U);
        }

        for (uint8_t bit = 8U; bit > 0U; bit--) {

            if ((remainder & 0x8000U) != 0U) {
                remainder = (remainder << 1U) ^ CRC_POLY;
            } else {
                remainder <<= 1U;
            }
        }
    }

    const auto crc_calc = static_cast<uint8_t>((remainder >> 12U) & CRC_MASK);
    return crc_calc == crc_read;
}
