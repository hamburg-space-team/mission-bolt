#include "ms5611.hpp"

Result<void> MS5611::init(CmsisI2CBus* bus, uint8_t addr, MS5611Osr osr, delay_fn delay) {
    this->bus = bus;
    this->addr = addr;
    this->osr = osr;
    this->delay_ms = delay;

    if (auto r = reset(); !r) {
        return r;
    }

    if (delay_ms != nullptr) {
        delay_ms(3U);
    }

    if (auto r = read_prom(); !r) {
        return r;
    }

    if (!prom_crc_ok()) {
        return std::unexpected(Error::PROTOCOL_ERROR);
    }
    return {};
}

Result<MS5611Result> MS5611::read() {
    if (is_failed()) {
        return std::unexpected(Error::DISABLED);
    }

    auto result = read_sample();
    if (!result) {
        register_failure();
        return result;
    }
    clear_failures();
    return result;
}

Result<void> MS5611::reset() {
    return bus->write(addr, &CMD_RESET, 1U);
}

Result<void> MS5611::read_prom() {
    for (uint8_t i = 0U; i < COEFF_COUNT; i++) {
        const auto reg = static_cast<uint8_t>(PROM_BASE_ADDR + (i * 2U));

        std::array<uint8_t, 2> buf{};
        if (auto r = bus->write_read(addr, &reg, 1U, buf.data(), buf.size()); !r) {
            return r;
        }

        coeff[i] = static_cast<uint16_t>((static_cast<uint16_t>(buf[0]) << 8U) | static_cast<uint16_t>(buf[1]));
    }

    coeff_read = true;
    return {};
}

Result<uint32_t> MS5611::read_adc(uint8_t cmd) {
    if (auto r = bus->write(addr, &cmd, 1U); !r) {
        return std::unexpected(r.error());
    }

    if (delay_ms != nullptr) {
        delay_ms(CONV_TIME_MS[static_cast<uint8_t>(osr)]);
    }

    if (auto r = bus->write(addr, &CMD_ADC_READ, 1U); !r) {
        return std::unexpected(r.error());
    }

    std::array<uint8_t, 3> buf{};
    if (auto r = bus->read(addr, buf.data(), sizeof(buf)); !r) {
        return std::unexpected(r.error());
    }

    return (static_cast<uint32_t>(buf[0]) << 16U) | (static_cast<uint32_t>(buf[1]) << 8U) |
           static_cast<uint32_t>(buf[2]);
}

Result<MS5611Result> MS5611::read_sample() {
    if (!coeff_read) {
        if (auto r = read_prom(); !r) {
            return std::unexpected(r.error());
        }
    }

    const auto temp_cmd = static_cast<uint8_t>(CMD_CONVERT_D2 | (static_cast<uint8_t>(osr) * 2U));
    auto d2 = read_adc(temp_cmd);
    if (!d2) {
        return std::unexpected(d2.error());
    }

    const auto pres_cmd = static_cast<uint8_t>(CMD_CONVERT_D1 | (static_cast<uint8_t>(osr) * 2U));
    auto d1 = read_adc(pres_cmd);
    if (!d1) {
        return std::unexpected(d1.error());
    }

    if (*d1 == 0U || *d2 == 0U) {
        return std::unexpected(Error::PROTOCOL_ERROR);
    }

    return MS5611Result{.d1 = *d1, .d2 = *d2};
}

bool MS5611::prom_crc_ok() const {
    std::array<uint16_t, COEFF_COUNT> prom = coeff;

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
