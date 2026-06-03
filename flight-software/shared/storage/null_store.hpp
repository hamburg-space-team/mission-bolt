#pragma once

#include "store.hpp"

/// No-op Store for bring-up, and any flight target
/// configured to run without persistent storage. Every write is
/// silently dropped; is_mounted() reports true so downstream code
/// (status payloads, branching on sd_status) treats storage as
/// healthy and doesn't trip the error path.
///
/// Use case: bring a controller up on the bench without an SD card
/// in the slot. Wire one of these into main.cpp instead of an SdStore
/// and the tick loop runs end-to-end without touching SDMMC.
///
/// @ingroup storage
class NullStore final : public Store {
  public:
    [[nodiscard]] Result<void> init() override {
        return {};
    }

    [[nodiscard]] Result<void> write(const uint8_t* /*data*/, uint8_t /*len*/) override {
        return {};
    }

    [[nodiscard]] bool drain_one() override {
        return false; // never any queued work
    }

    [[nodiscard]] Result<void> flush() override {
        return {};
    }

    [[nodiscard]] bool is_mounted() const override {
        return true;
    }

    [[nodiscard]] uint16_t dropped_count() const override {
        return 0U;
    }
};
