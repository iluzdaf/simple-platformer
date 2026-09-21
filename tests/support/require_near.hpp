#pragma once

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// Requires a float to be within the tolerance most tests use. It is a macro rather than a
// function so that a failure reports the test's own line. A test that needs a different
// tolerance writes the WithinAbs check itself, so the choice is visible where it is made.
#define REQUIRE_NEAR(actual, expected)                                                             \
    REQUIRE_THAT((actual), Catch::Matchers::WithinAbs((expected), 0.0001F))
