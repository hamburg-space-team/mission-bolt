#include "tmp117.hpp"
#include "cmsis_i2c_bus.hpp"

Result<void> TMP117::init(CmsisI2CBus* bus, uint8_t addr) {
    this->bus = bus;
    this->addr = addr;

    auto dev_id = this->bus->read_reg16(this->addr, REG_DEV_ID);
    if (!dev_id) {
        return std::unexpected(dev_id.error());
    }

    if ((*dev_id & DEV_ID_MASK) != DEV_ID_EXPECTED) {
        return std::unexpected(Error::PROTOCOL_ERROR);
    }

    return this->bus->write_reg16(this->addr, REG_CONFIG, CONFIG_CONTINUOUS);
}

Result<int16_t> TMP117::read() {
    if (is_failed()) {
        return std::unexpected(Error::DISABLED);
    }

    auto raw_u = this->bus->read_reg16(this->addr, REG_TEMP);
    if (!raw_u) {
        register_failure();
        return std::unexpected(raw_u.error());
    }

    clear_failures();
    return static_cast<int16_t>(*raw_u);
}
