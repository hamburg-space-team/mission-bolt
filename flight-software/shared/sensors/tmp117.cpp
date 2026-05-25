#include "tmp117.hpp"
#include "cmsis_i2c_bus.hpp"

int TMP117::init(CmsisI2CBus* bus, uint8_t addr) {
    this->bus = bus;
    this->addr = addr;

    uint16_t dev_id = 0U;

    if (this->bus->read_reg16(addr, REG_DEV_ID, &dev_id) < 0) {
        return -1;
    }

    if ((dev_id & DEV_ID_MASK) != DEV_ID_EXPECTED) {
        return -1;
    }

    return this->bus->write_reg16(addr, REG_CONFIG, CONFIG_CONTINUOUS);
}

int TMP117::read(int16_t* raw) {
    if (raw == nullptr || is_failed()) {
        return -1;
    }

    uint16_t raw_u = 0U;

    const int ret = this->bus->read_reg16(addr, REG_TEMP, &raw_u);

    if (ret < 0) {
        register_failure();
        return ret;
    }

    clear_failures();

    *raw = static_cast<int16_t>(raw_u);

    return 0;
}