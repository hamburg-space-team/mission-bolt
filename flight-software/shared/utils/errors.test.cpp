#include "utils/errors.hpp"

#include <catch2/catch_test_macros.hpp>

// ---------------------------------------------------------------------------
// Fake three-level driver chain, mirroring the flight pattern:
//   origin (bus wait)  ->  driver helper  ->  driver operation
// ---------------------------------------------------------------------------
namespace {

    Result<void> bus_wait() { // origin
        return fail(ErrorCode::TIMEOUT, Step::I2C_WAIT_COMPLETE, __LINE__);
    }

    Result<uint8_t> vreg_read() { // forwarding level 1
        if (auto r = bus_wait(); !r) {
            return mark(r.error(), Step::SPEC_VREG_READ);
        }
        return 0U;
    }

    Result<void> read_dies() { // forwarding level 2
        if (auto r = vreg_read(); !r) {
            return mark(r.error(), Step::SPEC_READ_DIES);
        }
        return {};
    }

} // namespace

TEST_CASE("fail() creates the origin entry", "[error-trace]") {
    const auto r = bus_wait();
    REQUIRE_FALSE(r.has_value());

    const Error& e = r.error();
    CHECK(e.code == ErrorCode::TIMEOUT);
    CHECK(e.depth == 1U);
    CHECK(e.trace[0] == Step::I2C_WAIT_COMPLETE);
    CHECK(e.line != 0U);
    CHECK_FALSE(e.truncated);
}

TEST_CASE("mark() appends the chain inner -> outer", "[error-trace]") {
    const auto r = read_dies();
    REQUIRE_FALSE(r.has_value());

    const Error& e = r.error();
    CHECK(e.code == ErrorCode::TIMEOUT); // cause survives unchanged
    CHECK(e.depth == 3U);
    CHECK(e.trace[0] == Step::I2C_WAIT_COMPLETE); // origin
    CHECK(e.trace[1] == Step::SPEC_VREG_READ);
    CHECK(e.trace[2] == Step::SPEC_READ_DIES);
    CHECK(e.trace[3] == Step::NONE); // untouched tail
    CHECK_FALSE(e.truncated);
}

TEST_CASE("mark() truncates instead of overflowing", "[error-trace]") {
    Error e = make_error(ErrorCode::BUS_ERROR, Step::I2C_WAIT_COMPLETE);
    for (int i = 0; i < 10; i++) {
        e = mark(e, Step::SPEC_VREG_READ).error();
    }

    CHECK(e.depth == TRACE_DEPTH); // clamped, never past the array
    CHECK(e.truncated);
    for (uint8_t i = 1U; i < TRACE_DEPTH; i++) {
        CHECK(e.trace[i] == Step::SPEC_VREG_READ);
    }
    // Repeated identical steps (poll loops) are legal entries.
}

TEST_CASE("error compares by cause", "[error-trace]") {
    const auto r = read_dies();
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == ErrorCode::TIMEOUT);
    CHECK_FALSE(r.error() == ErrorCode::BUS_ERROR);
}

TEST_CASE("ErrorClock stamps the origin timestamp", "[error-trace]") {
    // No clock (host default): timestamp reads 0.
    ErrorClock::now_us = nullptr;
    CHECK(make_error(ErrorCode::TIMEOUT, Step::SD_WRITE).timestamp_us == 0U);

    // Fake clock: fail()/make_error() capture the moment of occurrence.
    ErrorClock::now_us = []() -> uint32_t { return 123456U; };
    const auto e = fail(ErrorCode::TIMEOUT, Step::SD_WRITE, __LINE__).error();
    CHECK(e.timestamp_us == 123456U);
    ErrorClock::now_us = nullptr;
}

// constexpr path: mark() is usable at compile time (bounded, no clock).
namespace {
    constexpr Error K_BASE{.timestamp_us = 0U,
                           .line = 42U,
                           .code = ErrorCode::PROTOCOL_ERROR,
                           .depth = 1U,
                           .truncated = false,
                           .trace = {Step::TMP_ID_CHECK}};
    constexpr Error K_MARKED = mark(K_BASE, Step::TMP_READ).error();
    static_assert(K_MARKED.depth == 2U);
    static_assert(K_MARKED.trace[0] == Step::TMP_ID_CHECK);
    static_assert(K_MARKED.trace[1] == Step::TMP_READ);
    static_assert(!K_MARKED.truncated);
} // namespace

TEST_CASE("Error stays small enough for every Result<T>", "[error-trace]") {
    STATIC_CHECK(sizeof(Error) <= 16U);
}
