#pragma once

#include <bolt/wire/types.hpp>

#include <cstdint>
#include <optional>
#include <span>

class NodeComputer;

/// Node-local self-test: a table of steps, one invocation per tick.
/// The BTC sequencer decides WHOSE turn it is; a Runner decides which
/// step of its own node runs next. Every node owns a Runner
///
/// @ingroup core
namespace SelfTest {

    /// One step invocation. nullopt = still running, call again next tick;
    /// `first` marks the step's first tick (reset multi-tick phase state).
    /// `node` is the owning computer (steps downcast); `data` is the raw
    /// diagnostic shipped in the *_TEST packet, judged on ground
    using StepFn = std::optional<PacketProtocol::TestResult> (*)(NodeComputer& node, bool first, uint32_t& data);

    struct Step {
        StepFn run; // test_id = index into the table, no separate id to drift
    };

    /// What the node reports for the step it just ran
    struct Report {
        uint8_t test_id;
        PacketProtocol::TestResult result;
        bool last;
        uint32_t data;
    };

    /// @ingroup core
    class Runner {
      public:
        constexpr explicit Runner(NodeComputer& node_in) noexcept : node(&node_in) {
        }

        /// Begin a run from step 0 (restarts one in progress). Table passed
        /// here, not the ctor: it comes from a virtual on the owning computer
        void start(std::span<const Step> steps_in) noexcept {
            steps = steps_in;
            next = 0U;
            in_step = false;
            step_ticks = 0U;
            running = true;
        }

        /// Drop a run mid-way; the next start() begins again at step 0
        void abort() noexcept {
            running = false;
            in_step = false;
            step_ticks = 0U;
        }

        [[nodiscard]] bool active() const noexcept {
            return running;
        }

        /// Run (or continue) the next step; nullopt when idle or still in
        /// progress. The caller emits the Report as its *_TEST packet
        std::optional<Report> step() noexcept {
            if (!running) {
                return std::nullopt;
            }

            // Empty table still reports one SKIPPED: the sequencer advances
            // on `last`, and a silent node looks exactly like a hung one
            if (steps.empty()) {
                running = false;
                return Report{.test_id = 0U, .result = PacketProtocol::TestResult::SKIPPED, .last = true, .data = 0U};
            }

            const uint8_t id = next;
            uint32_t data = 0U;
            const bool first = !in_step;
            in_step = true;

            auto verdict = steps[id].run(*node, first, data);
            if (!verdict) {
                step_ticks++;
                if (step_ticks < MAX_STEP_TICKS) {
                    return std::nullopt; // no report this tick
                }
                // a stuck step must never stall the sequence
                verdict = PacketProtocol::TestResult::FAIL;
            }

            in_step = false;
            step_ticks = 0U;
            next++;

            const bool last = next >= steps.size();
            if (last) {
                running = false;
            }
            return Report{.test_id = id, .result = *verdict, .last = last, .data = data};
        }

      private:
        /// Per-step in-progress cap, 10 s at 25 Hz (longest legit wait:
        /// a 160-cycle spectrometer integration, ~23 ticks)
        static constexpr uint16_t MAX_STEP_TICKS = 250U;

        std::span<const Step> steps{};
        NodeComputer* node;
        uint8_t next = 0U;
        uint16_t step_ticks = 0U;
        bool in_step = false;
        bool running = false;
    };

} // namespace SelfTest
