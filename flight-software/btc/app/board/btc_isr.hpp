#pragma once

class BtcComputer;

/// ISR trampolines for the BTC
/// @ingroup apps
namespace BtcIsr {
    void bind(BtcComputer& computer) noexcept;

} // namespace BtcIsr
