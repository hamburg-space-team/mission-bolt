#pragma once

#include "errors.hpp" // ErrorClock::now_us (shared us clock)

#include <array>
#include <cstdint>

/// Lightweight per-scope WCET timing against the 25 Hz (40 ms) tick budget.
/// A scope records its worst-case duration into a fixed table; the node
/// downlinks the maxima at 1 Hz and resets, so each window shows its peak.
/// Uses the shared us clock, so it is portable across all nodes (no DWT) and
/// deliberately separate from the DWT helpers in the Timing namespace.
///
/// @ingroup utils
namespace Wcet {

    /// Generic per-node work categories. WIRE-STABLE: fixed order (index =
    /// wire slot); the ground maps these to labels + budgets. Universal slots
    /// (TICK/READ/SEND/STORE) mean the same on every node; CFG/DRIVE cover
    /// device control / actuator output and stay 0 on nodes that lack them.
    enum class Point : uint8_t {
        TICK = 0,  ///< whole on_(experiment_)tick
        READ = 1,  ///< sensor readout (e.g. AS7265x dies)
        CFG = 2,   ///< device configure / start (integration, one-shot)
        DRIVE = 3, ///< actuator output (LED writes)
        SEND = 4,  ///< build + enqueue downlink
        STORE = 5, ///< storage / flash write
        COUNT = 6,
    };

    inline constexpr uint8_t POINT_COUNT = static_cast<uint8_t>(Point::COUNT);

    /// Fixed, no-alloc timing table. record() sums a scope into the CURRENT
    /// tick's accumulator; tick_end() folds that sum into the window MAX and
    /// clears it, so each slot shows the worst *tick* in the window - correct
    /// even when a category fires several times per tick. reset() starts a new
    /// window after the maxima are sent.
    struct Timing {
        std::array<uint16_t, POINT_COUNT> max_us{}; // worst tick in the window
        std::array<uint32_t, POINT_COUNT> acc_us{}; // sum within the current tick

        void record(Point p, uint32_t us) noexcept {
            this->acc_us[static_cast<uint8_t>(p)] += us;
        }
        void tick_end() noexcept {
            for (uint8_t i = 0U; i < POINT_COUNT; i++) {
                const auto v = static_cast<uint16_t>(this->acc_us[i] > 0xFFFFU ? 0xFFFFU : this->acc_us[i]);
                if (v > this->max_us[i]) {
                    this->max_us[i] = v;
                }
                this->acc_us[i] = 0U;
            }
        }
        void reset() noexcept {
            this->max_us.fill(0U);
        }
    };

    /// RAII scope timer: on destruction records the elapsed us into `t` for
    /// `p`. No-op when the us clock is unset (host unit tests).
    class Scope {
      public:
        Scope(Timing& table, Point point) noexcept
            : table(table), point(point), start(ErrorClock::now_us != nullptr ? ErrorClock::now_us() : 0U) {
        }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        Scope(Scope&&) = delete;
        Scope& operator=(Scope&&) = delete;
        ~Scope() noexcept {
            if (ErrorClock::now_us != nullptr) {
                this->table.record(this->point, ErrorClock::now_us() - this->start);
            }
        }

      private:
        Timing& table; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        Point point;
        uint32_t start;
    };

} // namespace Wcet

/// Time the enclosing scope into `reg` (a Wcet::Timing) for point P.
#define BOLT_TIME(reg, P) ::Wcet::Scope _bolt_scope_##P((reg), ::Wcet::Point::P)
