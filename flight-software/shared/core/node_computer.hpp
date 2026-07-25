#pragma once

#include "boot_state.hpp"
#include "cmsis_i2c_bus.hpp"
#include "errors.hpp"
#include "flight_computer.hpp"
#include "ms5611.hpp"
#include "self_test.hpp"
#include "status_leds.hpp"
#include "store.hpp"
#include "tmp117.hpp"
#include "wcet.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

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
    /// `err` is the Error that caused it - subclasses turn it into a
    /// FAULT packet with the full step trace (ADR-012); the base only
    /// latches the LED code.
    virtual void report_fault(StatusLeds::Fault code, const Error& err) {
        (void)err;
        leds.set_fault(code);
    }

    /// One-shot poll for a runtime device latch
    void poll_device_fault(DeviceBase& dev, StatusLeds::Fault code) {
        if (dev.is_failed()) {
            report_fault(code, dev.last_error());
        }
    }

    void retry_failed_devices();

    virtual void retry_extra_devices() {
    }

    // Self-test step bodies for the sensors every node carries; each node
    // lists them first, so test_id 0..2 means the same on every node
    static std::optional<PacketProtocol::TestResult> step_tmp_whoami(NodeComputer& node, bool first,
                                                                     uint32_t& data) noexcept;
    static std::optional<PacketProtocol::TestResult> step_tmp_read(NodeComputer& node, bool first,
                                                                   uint32_t& data) noexcept;
    static std::optional<PacketProtocol::TestResult> step_baro_prom(NodeComputer& node, bool first,
                                                                    uint32_t& data) noexcept;

    /// Drain ring-buffered SD writes during the idle phase
    void on_drain(uint32_t deadline_ms) override;

    static constexpr uint16_t TX_BUF_SIZE = 64U;
    static constexpr uint32_t MIN_TIME_FOR_WRITE_MS = 5U;

    CmsisI2CBus& i2c; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    MS5611 baro;
    TMP117 tmp;

    Store& storage; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    StatusLeds leds;
    BootState::State boot;

    std::array<uint8_t, TX_BUF_SIZE> tx_buf{};

    Wcet::Timing timings{};
};
