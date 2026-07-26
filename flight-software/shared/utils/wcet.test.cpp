#include "utils/wcet.hpp"
#include "utils/errors.hpp"

#include <catch2/catch_test_macros.hpp>

// Interval arithmetic across the CYCCNT wrap. us_now() (cycles / 80) wraps at
// 2^32/80 - not a power of two - so subtracting two us values is wrong across
// a wrap. The fix subtracts in the cycle domain and divides last; these tests
// fail against the old us-domain subtraction.

namespace {

    constexpr uint32_t CPU = 80U; // cycles per us at 80 MHz

    // Fake cycle counter the Scope samples: first ctor, then dtor.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    uint32_t fake_cycles[2] = {0U, 0U};
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    int fake_idx = 0;

    uint32_t fake_now_cycles() {
        return fake_cycles[fake_idx++ & 1];
    }

    void install_fake(uint32_t start, uint32_t end) {
        fake_cycles[0] = start;
        fake_cycles[1] = end;
        fake_idx = 0;
        ErrorClock::now_cycles = fake_now_cycles;
        ErrorClock::cycles_per_us = CPU;
    }

    void uninstall_fake() {
        ErrorClock::now_cycles = nullptr;
        ErrorClock::cycles_per_us = 1U;
    }

} // namespace

TEST_CASE("us_between is exact across the counter wrap") {
    ErrorClock::cycles_per_us = CPU;

    // 25 us spanning the wrap: 2000 cycles ending 0x400 past zero
    const uint32_t start = 0xFFFFFFFFU - 1799U; // 1800 cycles below the wrap
    const uint32_t end = 200U;
    CHECK(ErrorClock::us_between(start, end) == 25U);

    // the same interval in the us domain would be ~4.24e9
    const uint32_t start_us = start / CPU;
    const uint32_t end_us = end / CPU;
    CHECK(end_us - start_us > 4'000'000'000U); // the old bug, for the record

    // no wrap: plain case still exact
    CHECK(ErrorClock::us_between(8'000U, 16'000U) == 100U);

    uninstall_fake();
}

TEST_CASE("Wcet::Scope spanning the wrap records the true duration") {
    Wcet::Timing table{};

    install_fake(0xFFFFFFFFU - 1799U, 200U); // 2000 cycles = 25 us across the wrap
    {
        Wcet::Scope scope(table, Wcet::Point::READ);
    }
    table.tick_end();

    // old code: 0xFFFF (saturated garbage) held by the max-hold
    CHECK(table.max_us[static_cast<uint8_t>(Wcet::Point::READ)] == 25U);

    uninstall_fake();
}

TEST_CASE("Wcet::Scope records nothing without a clock") {
    Wcet::Timing table{};
    uninstall_fake();
    {
        Wcet::Scope scope(table, Wcet::Point::READ);
    }
    table.tick_end();
    CHECK(table.max_us[static_cast<uint8_t>(Wcet::Point::READ)] == 0U);
}
