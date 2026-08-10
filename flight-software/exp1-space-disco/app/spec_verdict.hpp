#pragma once

#include <cstdint>

/// Verdict rule for one lit spectrometer step of the EXP1 self-test.
///
/// The 18-channel sum drifts slowly upward while a sweep runs, so a bare
/// `lit > dark` turns "no light at all" into a coin flip: the steps measured
/// before the drift adds a count read FAIL, the rest read PASS, and the
/// pattern moves from run to run. Demanding a margin makes a dead LED driver
/// fail unanimously, which is the answer the operator can act on.
///
/// The window is bounded on both sides by bench measurements. An unlit sweep
/// never moved more than 5 % of its dark reference; the weakest LED we must
/// still accept is IR 940 nm, at the edge of the AS7265x range, measured at
/// 1.15x dark while working.
namespace SpecVerdict {

/// Share of the dark reference the lit sum must exceed: 1/8 = 12.5 %.
inline constexpr uint32_t MARGIN_DIVISOR = 8U;

/// Floor in raw counts, for dark references too small for the share to
/// clear the drift on its own.
inline constexpr uint32_t MIN_DELTA = 12U;

/// True when `sum` is brighter than `dark` by more than the drift.
[[nodiscard]] constexpr bool lit(uint32_t sum, uint32_t dark) noexcept {
    const uint32_t share = dark / MARGIN_DIVISOR;
    return sum > dark + (share > MIN_DELTA ? share : MIN_DELTA);
}

} // namespace SpecVerdict
