#pragma once

#include "errors.hpp"

#include <cstdint>

/// @defgroup storage Storage

/// Abstract on-board log storage. The flight controllers (BTC, EXP1-3)
/// know only about this interface; the concrete backend is chosen in
/// the per-target main.cpp and passed by reference. This lets us swap
/// to a soldered flash chip later without touching the per-tick code.
///
/// Implementations:
///   - SdStore   - LittleFS on microSD via SDMMC (current flight backend)
///   - NullStore - no-op for bring-up / bench tests
///   - (future)  FlashStore - SPI / QSPI NOR flash
///
/// Lifecycle:
///   1. construct with whatever handle the backend needs
///   2. init() once at boot (mount / format / open)
///   3. write() on every packet, flush() on a 1 Hz cadence + critical
///      events (ADR-004)
///
/// Non-copyable, non-movable: the instance lives at a fixed address for
/// the lifetime of the program.
///
/// @ingroup storage
class Store {
  public:
    Store() = default;
    virtual ~Store() = default;

    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;
    Store(Store&&) = delete;
    Store& operator=(Store&&) = delete;

    /// One-time mount / format / open. Call once at boot.
    [[nodiscard]] virtual Result<void> init() = 0;

    /// Append raw bytes to the log. Returns immediately after the data
    /// hits the in-RAM cache; durability is only guaranteed after
    /// flush().
    [[nodiscard]] virtual Result<void> write(const uint8_t* data, uint8_t len) = 0;

    /// Commit buffered writes to durable storage.
    [[nodiscard]] virtual Result<void> flush() = 0;

    /// True once init() has succeeded and writes are accepted.
    [[nodiscard]] virtual bool is_mounted() const = 0;
};
