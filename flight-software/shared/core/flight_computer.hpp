#pragma once

#include "packet_builder.hpp"
#include "platform.hpp"

#include <cstdint>

class FlightComputer {
  public:
    explicit FlightComputer(const Platform& platform) noexcept;
    virtual ~FlightComputer() = default;

    FlightComputer(const FlightComputer&) = delete;
    FlightComputer& operator=(const FlightComputer&) = delete;
    FlightComputer(FlightComputer&&) = delete;
    FlightComputer& operator=(FlightComputer&&) = delete;

    [[noreturn]] void run() noexcept;

  protected:
    virtual void on_init() = 0;
    virtual void on_tick(uint32_t tick_start_us, uint16_t missed_periods) = 0;

    const Platform& platform;
    PacketProtocol::PacketBuilder pkt;

  private:
    static constexpr uint32_t LOOP_PERIOD_MS = 40U;
};
