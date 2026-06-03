#pragma once

#include "stm32l4xx_hal.h"
#include <cstdint>

/// @defgroup timing Timing helpers

/// DWT cycle-counter helpers. Used for packet timestamps and latency
/// budgeting.
///
/// @ingroup timing
namespace Timing {

    /// Enable the DWT cycle counter and reset it to zero. Call once at
    /// boot before any us_now() call - FlightComputer::run() does this.
    inline void init_dwt() {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0U;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }

    /// Microseconds since init_dwt(). Wraps after ~71s at 216 MHz (BTC
    /// F756) or ~37s at 80 MHz (EXP L476). Use for deltas only, never
    /// absolute time.
    inline uint32_t us_now() {
        return DWT->CYCCNT / (SystemCoreClock / 1000000U);
    }

    /// Microseconds elapsed since a reference captured via us_now().
    inline uint32_t us_since(uint32_t ref_us) {
        return us_now() - ref_us;
    }

} // namespace Timing
