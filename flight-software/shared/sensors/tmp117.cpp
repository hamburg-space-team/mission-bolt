#include "tmp117.hpp"
#include "cmsis_i2c_bus.hpp"

Result<void> TMP117::init(CmsisI2CBus* bus, uint8_t addr) {
    this->bus = bus;
    this->addr = addr;

    auto dev_id = this->bus->read_reg16(this->addr, REG_DEV_ID);
    if (!dev_id) {
        return mark(dev_id.error(), Step::TMP_INIT);
    }

    if ((*dev_id & DEV_ID_MASK) != DEV_ID_EXPECTED) {
        return fail(ErrorCode::PROTOCOL_ERROR, Step::TMP_ID_CHECK, __LINE__);
    }

    if (auto r = this->bus->write_reg16(this->addr, REG_CONFIG, CONFIG_CONTINUOUS); !r) {
        return mark(r.error(), Step::TMP_CONFIG);
    }
    return {};
}

Result<int16_t> TMP117::read() {
    if (is_failed()) {
        return fail(ErrorCode::DISABLED, Step::TMP_READ, __LINE__);
    }

    auto raw_u = this->bus->read_reg16(this->addr, REG_TEMP);
    if (!raw_u) {
        const auto marked = mark(raw_u.error(), Step::TMP_READ);
        register_failure(marked.error());
        return marked;
    }

    clear_failures();
    return static_cast<int16_t>(*raw_u);
}
