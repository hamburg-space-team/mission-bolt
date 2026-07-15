#pragma once

#include "errors.hpp"

#include <array>
#include <cstdint>

/// Lightweight per-scope WCET timing against the 25 Hz (40 ms) tick budget
///
/// @ingroup utils
namespace Wcet {

    /// Generic per-node work categories
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

#define BOLT_TIME(reg, P) ::Wcet::Scope _bolt_scope_##P((reg), ::Wcet::Point::P)
