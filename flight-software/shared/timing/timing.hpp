#pragma once

#include "stm32l4xx_hal.h"
#include <cstdint>

namespace Timing {

    // Initialize DWT cycle counter.
    // Call once at boot before any us_now() calls.
    inline void init_dwt() {
        // Enable DWT
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

        // Reset cycle counter
        DWT->CYCCNT = 0U;

        // Enable cycle counter
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }

    // Returns microseconds since DWT was initialized.
    // Wraps after ~71 seconds at 216 MHz (BTC F756) or ~37s at 80 MHz (EXP L476).
    // Use delta measurements only -- not absolute time.
    inline uint32_t us_now() {
        return DWT->CYCCNT / (SystemCoreClock / 1000000U);
    }

    // Returns microseconds elapsed since a reference point.
    inline uint32_t us_since(uint32_t ref_us) {
        return us_now() - ref_us;
    }

} // namespace Timing