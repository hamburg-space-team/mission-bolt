#pragma once

#include "boot_state.hpp"
#include "cmsis_i2c_bus.hpp"
#include "flight_computer.hpp"
#include "ms5611.hpp"
#include "status_leds.hpp"
#include "store.hpp"
#include "tmp117.hpp"

#include <array>
#include <cstdint>

/// Intermediate base for all BOLT nodes (BTC + EXPs). Owns the sensors
/// present on every board: MS5611 (F-80, D-350) and TMP117 (F-70,
/// D-340). Owns status LEDs and boot state.
///
/// @ingroup core
class NodeComputer : public FlightComputer {
  protected:
    explicit NodeComputer(const Platform& platform, CmsisI2CBus& i2c, Store& storage) noexcept;

    /// Initialize I2C bus and common sensors, then call init_extra_sensors().
    void init_sensors();

    /// Mount / open the configured Store. Call once from on_init(),
    /// before the main loop begins.
    void init_storage();

    /// Override to initialize board-specific sensors
    /// Called at the end of init_sensors() after common sensors are ready.
    virtual void init_extra_sensors() {
    }

    /// Called when a device fails to initialise (or latches at runtime).
    virtual void on_sensor_failed(StatusLeds::Fault code) {
        leds.set_fault(code);
    }

    /// Drain ring-buffered SD writes during the idle phase, respecting
    /// the per-op safety margin from ADR-004 (MIN_TIME_FOR_WRITE_MS).
    void on_drain(uint32_t deadline_ms) override;

    static constexpr uint16_t TX_BUF_SIZE = 64U;

    /// Cadence (in ticks) at which storage.flush() is called on the
    /// steady-state schedule.
    static constexpr uint8_t FLUSH_INTERVAL = 25U;

    /// The drain phase will not start a new lfs_file_write() if fewer milliseconds
    /// remain in the tick.
    static constexpr uint32_t MIN_TIME_FOR_WRITE_MS = 5U;

    CmsisI2CBus& i2c; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    MS5611 baro;
    TMP117 tmp;
    Store& storage; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    StatusLeds leds;
    BootState::State boot;
    std::array<uint8_t, TX_BUF_SIZE> tx_buf{};
};
