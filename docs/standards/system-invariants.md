# System Invariants

The principles and testable invariants that guide every architectural 
decision in BOLT THHOR. ADRs reference these. Code review uses them. 
When something feels wrong, check it against this list.

## The Setup

We build software for a payload that flies once. The science window is 
about 600 seconds. We cannot push a fix mid-flight. Every fault during 
that window translates to lost data.

Four principles follow from this.

## Principles

**P-1: Failure is normal.** Things go wrong. Sensors hang, controllers 
reset, SD writes fail. We design so that each fault degrades one piece 
rather than cascading through the system.

**P-2: SD card and downlink are complementary.** Science is recovered 
from SD after flight. The downlink is for live monitoring and the 
minimum-success criterion. We do not depend on the downlink for 
scientific success.

**P-3: Predictability beats performance.** Bounded time per operation, 
known memory sizes, named error paths. A 40 ms tick is slower than an 
event-driven system, but we can analyse it by reading the code.

**P-4: Honesty about what we know.** Invalid measurements are marked. 
Missing packets are flagged with a reason. We never silently substitute 
a fallback value. We would rather downlink less data we trust than more 
data of unknown provenance.

## Invariants

Seven testable properties that must hold in flight. Each one ties back 
to a principle.

**I-1: Deterministic tick budget (P-3).** The critical path of each 
tick fits in 40 ms, target 35 ms. IWDG enforces this as last resort.

**I-2: Bounded blocking on the critical path (P-1, P-3).** Every I2C, 
UART, and CAN call has a timeout. A misbehaving peripheral loses one 
tick of data, not the flight loop. SD writes are the documented 
exception and run off the critical path.

**I-3: No dynamic allocation after init (P-3).** All buffers allocated 
at compile time or in `on_init()`. No heap use in flight.

**I-4: Monotonic science timeline (P-1, P-4).** The tick counter only 
increases. Mid-flight reboots restore the tick from `.noinit` SRAM. 
Ground correlates everything by tick number; non-monotonic ticks 
silently corrupt the analysis.

**I-5: Loose inter-controller coupling (P-1).** The four flight 
computers do not depend on each other to do their own work. Each 
experiment continues if the BTC fails.

**I-6: No silent substitution (P-4).** Invalid sensor reads set their 
bit in `valid_mask`. Missing packets become `GAP_MARKER` with a reason. 
Ground can always tell "measured" from "missing or invalid."

**I-7: SD card is the survivor (P-2).** Every scientifically relevant 
measurement reaches the SD ringbuffer in the same tick it was produced. 
LittleFS gives power-cut-safe semantics. The SD log is the authoritative 
record after recovery.

## How We Use This

When we propose a change, we check it against the invariants. A change 
that breaks one needs strong justification.

In code review, we cite the invariant by number:

> The unbounded loop in `process_response()` violates I-2. Add a deadline.

In ADRs, we reference invariants the decision supports or trades against.

In code comments, sparingly, where the reason for a construct is not 
obvious:

```cpp
// I-1: refresh exactly once per tick. Refreshing elsewhere would 
// mask stalls in specific subsystems.
platform.kick_wdg();
```

## Maintenance

Changes to the invariants themselves need an ADR. Adding new invariants 
is welcome if they capture something real about the system that the 
current list misses.

For implementation details of each invariant, see the relevant ADR.