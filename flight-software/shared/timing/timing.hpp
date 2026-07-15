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

    /// Microseconds since init_dwt()
    inline uint32_t us_now() {
        return DWT->CYCCNT / (SystemCoreClock / 1000000U);
    }

    /// Microseconds elapsed since a reference captured via us_now().
    inline uint32_t us_since(uint32_t ref_us) {
        return us_now() - ref_us;
    }

} // namespace Timing
