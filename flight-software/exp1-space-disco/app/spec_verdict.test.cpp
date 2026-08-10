#include "spec_verdict.hpp"

#include <catch2/catch_test_macros.hpp>

// Every number below is a measured sweep off the bench, not an invented one:
// runs 38-44 had the LED drivers unconfigured, runs 25/34 had them working.

TEST_CASE("an unlit sweep fails every colour", "[exp1][selftest]") {
    // run 44: the baseline drifted 181 -> 186 across the seven steps, which
    // the old `lit > dark` rule scored as one FAIL and five PASSes
    CHECK_FALSE(SpecVerdict::lit(181, 181));
    CHECK_FALSE(SpecVerdict::lit(186, 181));
    // run 39: every step landed on exactly dark + 1
    CHECK_FALSE(SpecVerdict::lit(226, 225));
    // run 40: the widest unlit excursion we have seen, +18 on 400
    CHECK_FALSE(SpecVerdict::lit(418, 400));
    // run 38: drift downward, already failing before
    CHECK_FALSE(SpecVerdict::lit(251, 256));
}

TEST_CASE("a working LED passes, down to the weakest one", "[exp1][selftest]") {
    // run 34, dark 108: red and white are unambiguous
    CHECK(SpecVerdict::lit(245, 108));
    CHECK(SpecVerdict::lit(308, 108));
    // ... and IR 940 nm is the binding case at 1.15x dark
    CHECK(SpecVerdict::lit(124, 108));
    // run 25, dark 57: a small reference still admits a real LED
    CHECK(SpecVerdict::lit(136, 57));
    CHECK(SpecVerdict::lit(85, 57));
}

TEST_CASE("the floor covers dark references too small for the share", "[exp1][selftest]") {
    // run 43 had dark 23, where 12.5 % is 2 counts - less than the drift
    CHECK_FALSE(SpecVerdict::lit(31, 23));
    CHECK(SpecVerdict::lit(46, 23));
    // a zero reference must not make noise look like light
    CHECK_FALSE(SpecVerdict::lit(8, 0));
    CHECK(SpecVerdict::lit(13, 0));
}
