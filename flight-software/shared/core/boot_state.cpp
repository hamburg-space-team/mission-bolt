#include "boot_state.hpp"
#include "main.h"

// Persistent state in .noinit RAM - survives IWDG/soft resets, cleared by POR/BOR/pin reset.
namespace {
    constexpr uint32_t MAGIC_WORD = 0xB017BEEFU;

    struct PersistentState {
        uint32_t magic;
        uint16_t tick;
        uint16_t reboot_count;
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
        } else {
            state.tick_valid = false;
            state.reboot_count = 0U;
            persist.magic = 0U;
            persist.tick = 0U;
            persist.reboot_count = 0U;
        }

        return state;
    }

    void save_tick(uint16_t tick) {
        persist.magic = MAGIC_WORD;
        persist.tick = tick;
    }

} // namespace BootState
