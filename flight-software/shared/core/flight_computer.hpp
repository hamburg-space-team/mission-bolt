#pragma once

#include "packet_builder.hpp"
#include "platform.hpp"

#include <cstdint>

/// Top of the computer hierarchy. 40 ms = 25 Hz tick loop, no RTOS
/// (ADR-001). Derived classes override on_init() and on_tick(); the
/// base measures slip and kicks the IWDG.
///
/// 25 Hz satisfies the environmental measurement rates (P-10 >= 4 Hz,
/// P-40/P-50 25 Hz) and the autonomous-operation requirement O-10.
///
/// @ingroup core
class FlightComputer {
  public:
    explicit FlightComputer(const Platform& platform) noexcept;
    virtual ~FlightComputer() = default;

    FlightComputer(const FlightComputer&) = delete;
    FlightComputer& operator=(const FlightComputer&) = delete;
    FlightComputer(FlightComputer&&) = delete;
    FlightComputer& operator=(FlightComputer&&) = delete;

    /// Enter the 40 ms tick loop. Performs DWT init, calls on_init(),
    /// then alternates on_tick() with an IWDG kick. Never returns.
    [[noreturn]] void run() noexcept;

  protected:
    virtual void on_init() = 0;

    /// tick_start_us:  microsecond timestamp captured at tick start;
    ///                 stamp on every outbound packet.
    /// missed_periods: tick deadlines missed since the previous call.
    ///                 > 0 means scheduler overrun.
    virtual void on_tick(uint32_t tick_start_us, uint16_t missed_periods) = 0;

    /// Idle phase between on_tick() and kick_wdg()
    virtual void on_drain(uint32_t /*deadline_ms*/) {
    }

    const Platform& platform; // NOLINT - borrowed from owning main.cpp
    PacketProtocol::PacketBuilder pkt;

  private:
    static constexpr uint32_t LOOP_PERIOD_MS = 40U;
};
