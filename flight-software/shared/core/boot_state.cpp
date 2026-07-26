#include "boot_state.hpp"
#include "main.h"

// Persistent state in .noinit RAM - survives IWDG/soft resets, cleared by POR/BOR/pin reset.
namespace {
    constexpr uint32_t MAGIC_WORD = 0xB017FEEDU;

    struct PersistentState {
        uint32_t magic;
        uint16_t tick;
        uint16_t reboot_count;
        uint8_t mode;       // BootState::Mode
        uint8_t lo_latched; // 1 = LO seen; restores the LoTracker latch
        uint32_t lo_rtc_s;
    };

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    [[gnu::section(".noinit")]] PersistentState persist;
} // namespace

namespace BootState {

    State read() { // NOLINT(readability-function-cognitive-complexity)
        State state{};

        if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != 0U) {
            state.reason = PacketProtocol::BootReason::WATCHDOG;
        } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != 0U) {
            state.reason = PacketProtocol::BootReason::SOFT_RESET;
        } else {
            state.reason = PacketProtocol::BootReason::COLD_START;
        }
        __HAL_RCC_CLEAR_RESET_FLAGS();

        const bool is_cold = (state.reason == PacketProtocol::BootReason::COLD_START);
        const bool warm = (persist.magic == MAGIC_WORD) && !is_cold;

        if (warm) {
            state.recovered_tick = persist.tick;
            state.tick_valid = true;
            state.reboot_count = static_cast<uint16_t>(persist.reboot_count + 1U);
            persist.reboot_count = state.reboot_count;
            // Resume the mission mode: a reset in flight must not drop the
            // experiments back into TEST. Anything but a valid FLIGHT marker
            // falls back to TEST rather than trusting a stray byte
            state.mode = (persist.mode == static_cast<uint8_t>(Mode::FLIGHT)) ? Mode::FLIGHT : Mode::TEST;
            state.lo_latched = (persist.lo_latched == 1U);
            state.lo_rtc_s = persist.lo_rtc_s;
            // LO implies FLIGHT by system invariant - the latch outranks a
            // corrupted mode byte
            if (state.lo_latched) {
                state.mode = Mode::FLIGHT;
            }
        } else {
            state.tick_valid = false;
            state.reboot_count = 0U;
            // Cold start = powered up on the pad/bench: always TEST
            state.mode = Mode::TEST;
            state.lo_latched = false;
            state.lo_rtc_s = 0U;
            persist.magic = 0U;
            persist.tick = 0U;
            persist.reboot_count = 0U;
            persist.mode = static_cast<uint8_t>(Mode::TEST);
            persist.lo_latched = 0U;
            persist.lo_rtc_s = 0U;
        }

        return state;
    }

    void save_tick(uint16_t tick) {
        persist.magic = MAGIC_WORD;
        persist.tick = tick;
    }

    void save_mode(Mode mode) {
        persist.magic = MAGIC_WORD;
        persist.mode = static_cast<uint8_t>(mode);
    }

    void save_lo(uint32_t rtc_s) {
        persist.magic = MAGIC_WORD;
        persist.lo_latched = 1U;
        persist.lo_rtc_s = rtc_s;
    }

} // namespace BootState
