#include <catch2/catch_test_macros.hpp>

#include "ui/inventory_layout.hpp"

TEST_CASE("Inventory grid dimensions follow its slot capacity", "[app][inventory]")
{
    SECTION("one slot")
    {
        const auto layout = simple_platformer::makeInventoryGridLayout(1);
        REQUIRE(layout.columns == 1);
        REQUIRE(layout.rows == 1);
    }

    SECTION("six slots")
    {
        const auto layout = simple_platformer::makeInventoryGridLayout(6);
        REQUIRE(layout.columns == 3);
        REQUIRE(layout.rows == 2);
    }

    SECTION("eight slots")
    {
        const auto layout = simple_platformer::makeInventoryGridLayout(8);
        REQUIRE(layout.columns == 3);
        REQUIRE(layout.rows == 3);
    }

    SECTION("no slots")
    {
        const auto layout = simple_platformer::makeInventoryGridLayout(0);
        REQUIRE(layout.columns == 0);
        REQUIRE(layout.rows == 0);
    }
}
