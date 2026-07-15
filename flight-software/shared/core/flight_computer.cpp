#include "flight_computer.hpp"
#include "errors.hpp"
#include "timing.hpp"

FlightComputer::FlightComputer(const Platform& platform) noexcept : platform(platform) {
    ErrorClock::now_us = platform.tick_us;
    this->pkt.init();
}

[[noreturn]] void FlightComputer::run() noexcept {
    Timing::init_dwt();
    on_init();
    uint32_t next_ms = this->platform.tick_ms() + LOOP_PERIOD_MS;

    while (true) {
        const uint32_t now = this->platform.tick_ms();

        uint16_t missed = 0U;
        if (now >= next_ms) {
            missed = static_cast<uint16_t>((now - next_ms) / LOOP_PERIOD_MS);
            next_ms += (static_cast<uint32_t>(missed) + 1U) * LOOP_PERIOD_MS;
        } else {
            this->platform.delay_ms(next_ms - now);
            next_ms += LOOP_PERIOD_MS;
        }

        const uint32_t tick_start_us = this->platform.tick_us();
        on_tick(tick_start_us, missed);

        // Idle phase: drain pending IO
        on_drain(next_ms);

        this->platform.kick_wdg();
    }
}
