#include "ms5611.hpp"

int MS5611::init(CmsisI2CBus* bus, uint8_t addr, MS5611Osr osr, delay_fn delay) {
    this->bus = bus;
    this->addr = addr;
    this->osr = osr;
    this->delay_ms = delay;

    int ret = reset();

    if (ret < 0) {
        return ret;
    }

    if (delay_ms != nullptr) {
        delay_ms(3U);
    }

    ret = read_prom();

    if (ret < 0) {
        return ret;
    }

    return prom_crc_ok() ? 0 : -1;
}

int MS5611::read(MS5611Result* result) {
    if (result == nullptr || is_failed()) {
        return -1;
    }

    const int ret = read_sample(result);

    if (ret < 0) {
        register_failure();
    } else {
        clear_failures();
    }

    return ret;
}

int MS5611::reset() {
    return bus->write(addr, &CMD_RESET, 1U);
}

int MS5611::read_prom() {
    for (uint8_t i = 0U; i < COEFF_COUNT; i++) {

        const auto reg = static_cast<uint8_t>(PROM_BASE_ADDR + (i * 2U));

        std::array<uint8_t, 2> buf{};
        const int ret = bus->write_read(addr, &reg, 1U, buf.data(), buf.size());

        if (ret < 0) {
            return ret;
        }

        coeff[i] = static_cast<uint16_t>((static_cast<uint16_t>(buf[0]) << 8U) | static_cast<uint16_t>(buf[1]));
    }

    coeff_read = true;

    return 0;
}

int MS5611::read_adc(uint8_t cmd, uint32_t* value) {
    int ret = bus->write(addr, &cmd, 1U);

    if (ret < 0) {
        return ret;
    }

    if (delay_ms != nullptr) {

        delay_ms(CONV_TIME_MS[static_cast<uint8_t>(osr)]);
    }

    ret = bus->write(addr, &CMD_ADC_READ, 1U);

    if (ret < 0) {
        return ret;
    }

    std::array<uint8_t, 3> buf{};

    ret = bus->read(addr, buf.data(), sizeof(buf));

    if (ret < 0) {
        return ret;
    }

    *value =
        (static_cast<uint32_t>(buf[0]) << 16U) | (static_cast<uint32_t>(buf[1]) << 8U) | static_cast<uint32_t>(buf[2]);

    return 0;
}

int MS5611::read_sample(MS5611Result* result) {
    if (!coeff_read) {

        const int ret = read_prom();

        if (ret < 0) {
            return ret;
        }
    }

    const auto temp_cmd = static_cast<uint8_t>(CMD_CONVERT_D2 | (static_cast<uint8_t>(osr) * 2U));
    uint32_t d2 = 0U;

    int ret = read_adc(temp_cmd, &d2);

    if (ret < 0) {
        return ret;
    }

    const auto pres_cmd = static_cast<uint8_t>(CMD_CONVERT_D1 | (static_cast<uint8_t>(osr) * 2U));
    uint32_t d1 = 0U;

    ret = read_adc(pres_cmd, &d1);

    if (ret < 0) {
        return ret;
    }

    if (d1 == 0U || d2 == 0U) {
        return -1;
    }

    result->d1 = d1;
    result->d2 = d2;

    return 0;
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