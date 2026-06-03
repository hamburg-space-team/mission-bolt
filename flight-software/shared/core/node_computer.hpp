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

    /// Initialize I2C bus and common sensors, then call
    /// init_extra_sensors(). Call once from on_init().
    void init_sensors();

    /// Mount / open the configured Store. Call once from on_init(),
    /// before the main loop begins.
    void init_storage();

    /// Override to initialize board-specific sensors (e.g. ICM-42686-P
    /// on BTC and EXP3). Called at the end of init_sensors() after
    /// common sensors are ready.
    virtual void init_extra_sensors() {
    }

    /// Called by init_sensors() when a common sensor fails to
    /// initialise. Subclasses send a GAP_MARKER over their downlink
    /// (CAN or UART). I-6: no silent fallback values.
    virtual void on_sensor_failed() {
    }

    static constexpr uint16_t TX_BUF_SIZE = 64U;

    static constexpr uint8_t FLUSH_INTERVAL = 25U;

    CmsisI2CBus& i2c; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    MS5611 baro;
    TMP117 tmp;
    Store& storage; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    StatusLeds leds;
    BootState::State boot;
    std::array<uint8_t, TX_BUF_SIZE> tx_buf{};
};
