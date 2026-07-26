#pragma once

#include <bolt/wire/types.hpp>
#include <cstdint>

/// Boot recovery helpers (ADR-008). .noinit RAM survives IWDG / soft
/// resets; POR/BOR loses SRAM and a pin reset is treated as cold by
/// policy, so only watchdog/soft resets recover the persisted tick,
/// mode and reboot count.
///
/// @ingroup core
namespace BootState {

    /// Boot lands in TEST; LO switches to FLIGHT for good. Persisted in
    /// .noinit so a mid-flight reset comes back in FLIGHT; a real power-cycle
    /// clears it back to TEST
    enum class Mode : uint8_t {
        TEST = 0U,   ///< bench / pre-flight: experiments idle, test tick runs
        FLIGHT = 1U, ///< LO seen: experiments run
    };

    struct State {
        PacketProtocol::BootReason reason;
        uint16_t reboot_count;   // total non-cold-start resets since first power-on
        uint16_t recovered_tick; // tick count to resume from; only valid when tick_valid
        bool tick_valid;         // true if a recoverable watchdog/soft reset
        Mode mode;               // recovered on a warm reset, TEST on cold start
        bool lo_latched;         // LO had latched before the reset - restore the latch
        uint32_t lo_rtc_s;       // RTC stamp captured at LO, restored with the latch
    };

    /// Call once at startup. Reads RCC reset flags and .noinit RAM,
    /// clears reset flags. Requires: .noinit NOLOAD section in the
    /// linker script.
    State read();

    /// Persist tick into .noinit RAM. Call periodically (e.g. every
    /// 25 ticks).
    void save_tick(uint16_t tick);

    /// Persist the mission mode. Called on every transition - .noinit must
    /// always agree with what the SYNC broadcast says
    void save_mode(Mode mode);

    /// Persist the LO latch + its RTC stamp, once, when LO latches. Without it
    /// a warm reset re-runs the LO level fallback and re-zeroes the resumed tick
    void save_lo(uint32_t rtc_s);

} // namespace BootState
