#pragma once

#include <bolt/wire/types.hpp>
#include <cstdint>

/// Boot recovery helpers (ADR-008). .noinit RAM survives IWDG / soft
/// resets but is cleared by POR/BOR/pin resets, so we use it to
/// remember the last persisted tick and rebuild the downlink stream
/// after a brief MCU reset.
///
/// @ingroup core
namespace BootState {

    struct State {
        PacketProtocol::BootReason reason;
        uint16_t reboot_count;   // total non-cold-start resets since first power-on
        uint16_t recovered_tick; // tick count to resume from; only valid when tick_valid
        bool tick_valid;         // true if a recoverable watchdog/soft reset
    };

    /// Call once at startup. Reads RCC reset flags and .noinit RAM,
    /// clears reset flags. Requires: .noinit NOLOAD section in the
    /// linker script.
    State read();

    /// Persist tick into .noinit RAM. Call periodically (e.g. every
    /// 25 ticks).
    void save_tick(uint16_t tick);

} // namespace BootState
