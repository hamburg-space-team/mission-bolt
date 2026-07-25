#pragma once

#include "stm32l4xx_hal.h" // IWYU pragma: keep
#include <cstdint>

/// @defgroup timing Timing helpers

/// DWT cycle-counter helpers. Used for packet timestamps and latency
/// budgeting.
///
/// @ingroup timing
namespace Timing {

    /// Enable the DWT cycle counter
    inline void init_dwt() {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0U;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }

    /// Raw cycle counter, wraps at 2^32 (53.687 s at 80 MHz). For intervals:
    /// subtract two samples in the cycle domain, then ErrorClock::us_between
    inline uint32_t cycles_now() {
        return DWT->CYCCNT;
    }

    /// Microseconds since init_dwt(). Absolute stamp only - wraps at
    /// 2^32/80 = 53.687 s, which is NOT a power of two, so subtracting two
    /// us_now() values is wrong across a wrap. Never diff these
    inline uint32_t us_now() {
        return DWT->CYCCNT / (SystemCoreClock / 1000000U);
    }

} // namespace Timing
