#pragma once

#include "device_base.hpp"
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
/// Non-copyable, non-movable: the instance lives at a fixed address for
/// the lifetime of the program.
///
/// Inherits DeviceBase so a storage backend is handled uniformly with the
/// sensors: latched via disable() on an init() failure, retried on the 30 s
/// cooldown, recovered in place via init() + clear_latch() (a Store owns a HAL
/// handle plus buffers and is held by reference, so it cannot be swapped for a
/// fresh instance the way a sensor driver is).
///
/// @ingroup storage
class Store : public DeviceBase {
  public:
    Store() = default;
    virtual ~Store() = default;

    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;
    Store(Store&&) = delete;
    Store& operator=(Store&&) = delete;

    /// One-time mount / format / open. Call once at boot.
    [[nodiscard]] virtual Result<void> init() = 0;

    /// Enqueue raw bytes for later durable write. Microsecond cost;
    /// safe to call from the critical tick path. Returns Error::IO_ERROR
    /// if the ring buffer is full (the entry is dropped, dropped_count
    /// is incremented for the next status payload).
    [[nodiscard]] virtual Result<void> write(const uint8_t* data, uint8_t len) = 0;

    /// Pop one queued entry from the ring buffer and perform the
    /// backend write. Called from the framework's idle phase only when
    /// at least MIN_TIME_FOR_WRITE_MS of tick budget remains. Returns
    /// true if an entry was drained, false if the queue is empty (no
    /// further drain attempts this tick).
    [[nodiscard]] virtual bool drain_one() = 0;

    /// Commit buffered writes to durable storage. Does NOT drain the
    /// ring buffer; call after the drain phase (or trust the periodic
    /// drain to catch up first).
    [[nodiscard]] virtual Result<void> flush() = 0;

    /// True once init() has succeeded and writes are accepted.
    [[nodiscard]] virtual bool is_mounted() const = 0;

    /// Cumulative ring-buffer overflows since boot. Surfaced through
    /// the status payload so ground can see if production outran the
    /// drain (typically means SD card stalls were longer than the ring
    /// buffer can absorb).
    [[nodiscard]] virtual uint16_t dropped_count() const = 0;
};
