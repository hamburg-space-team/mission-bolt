#pragma once

#include "errors.hpp"
#include "store.hpp"

#include <cstdint>

/// Store for a board with no working storage backend. It never mounts:
/// init() fails as DISABLED so the framework latches it and ground sees
/// Fault::SD with sd_status bit 0 = 0, rather than a log that silently
/// goes nowhere (I-6).
///
/// Stands in while the FM boards move from SDMMC to QSPI flash and the
/// flash backend does not exist yet.
///
/// @ingroup storage
class NullStore final : public Store {
  public:
    NullStore() = default;

    [[nodiscard]] Result<void> init() override {
        return fail(ErrorCode::DISABLED, Step::SD_INIT, __LINE__);
    }

    [[nodiscard]] Result<void> write(const uint8_t* data, uint8_t len) override {
        (void)data;
        (void)len;
        this->drops = static_cast<uint16_t>(this->drops + 1U);
        return fail(ErrorCode::DISABLED, Step::SD_WRITE, __LINE__);
    }

    [[nodiscard]] bool drain_one() override {
        return false;
    }

    [[nodiscard]] Result<void> flush() override {
        return fail(ErrorCode::DISABLED, Step::SD_FLUSH, __LINE__);
    }

    [[nodiscard]] bool is_mounted() const override {
        return false;
    }

    [[nodiscard]] uint16_t dropped_count() const override {
        return this->drops;
    }

  private:
    uint16_t drops = 0U;
};
