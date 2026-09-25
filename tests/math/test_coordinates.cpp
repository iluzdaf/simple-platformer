#include <catch2/catch_test_macros.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "support/require_near.hpp"

namespace
{
    using simple_platformer::Aabb;

}

TEST_CASE("An AABB position is its top-left corner", "[math][coordinates]")
{
    const Aabb box{{10.0F, 20.0F}, {12.0F, 24.0F}};

    REQUIRE_NEAR(simple_platformer::centerOf(box).x, 16.0F);

    REQUIRE_NEAR(simple_platformer::centerOf(box).y, 32.0F);
    REQUIRE_NEAR(simple_platformer::feetOf(box).x, 16.0F);
    REQUIRE_NEAR(simple_platformer::feetOf(box).y, 44.0F);
}

TEST_CASE("An arbitrary-sized AABB can be placed by its feet", "[math][coordinates]")
{
    Aabb box{{0.0F, 0.0F}, {12.0F, 24.0F}};

    simple_platformer::placeFeetAt(box, {40.0F, 128.0F});

    REQUIRE_NEAR(box.position.x, 34.0F);

    REQUIRE_NEAR(box.position.y, 104.0F);
    REQUIRE_NEAR(simple_platformer::feetOf(box).x, 40.0F);
    REQUIRE_NEAR(simple_platformer::feetOf(box).y, 128.0F);
}

TEST_CASE("AABBs overlap when their areas intersect", "[math][aabb]")
{
    const Aabb first{{10.0F, 10.0F}, {10.0F, 10.0F}};
    const Aabb second{{15.0F, 15.0F}, {10.0F, 10.0F}};

    REQUIRE(simple_platformer::overlaps(first, second));
    REQUIRE(simple_platformer::overlaps(second, first));
}

TEST_CASE("AABBs that only touch at an edge do not overlap", "[math][aabb]")
{
    const Aabb first{{10.0F, 10.0F}, {10.0F, 10.0F}};
    const Aabb second{{20.0F, 10.0F}, {10.0F, 10.0F}};

    REQUIRE_FALSE(simple_platformer::overlaps(first, second));
}

TEST_CASE("An AABB contains points inside it and on its near edges", "[math][aabb]")
{
    const simple_platformer::Aabb box{{10.0F, 20.0F}, {4.0F, 6.0F}};

    REQUIRE(simple_platformer::contains(box, {12.0F, 23.0F}));
    REQUIRE(simple_platformer::contains(box, {10.0F, 20.0F}));
    REQUIRE_FALSE(simple_platformer::contains(box, {14.0F, 23.0F}));
    REQUIRE_FALSE(simple_platformer::contains(box, {12.0F, 26.0F}));
    REQUIRE_FALSE(simple_platformer::contains(box, {9.0F, 23.0F}));
}

TEST_CASE("Grid positions compare by both coordinates", "[math][coordinates]")
{
    using simple_platformer::GridPosition;

    REQUIRE(GridPosition{2, 3} == GridPosition{2, 3});
    REQUIRE(GridPosition{2, 3} != GridPosition{3, 2});
}

TEST_CASE("World and grid coordinates convert at tile boundaries", "[math][coordinates]")
{
    using simple_platformer::GridPosition;

    REQUIRE(simple_platformer::worldToGrid(16, {0.0F, 0.0F}) == GridPosition{0, 0});
    REQUIRE(simple_platformer::worldToGrid(16, {15.9F, 31.9F}) == GridPosition{0, 1});
    REQUIRE(simple_platformer::worldToGrid(16, {16.0F, 32.0F}) == GridPosition{1, 2});
    REQUIRE(simple_platformer::worldToGrid(16, {-0.1F, -16.1F}) == GridPosition{-1, -2});
    REQUIRE_NEAR(simple_platformer::gridToWorld(16, {2, 3}).x, 32.0F);
    REQUIRE_NEAR(simple_platformer::gridToWorld(16, {2, 3}).y, 48.0F);
}

TEST_CASE("The tile size scales every cell conversion", "[math][coordinates]")
{
    using simple_platformer::GridPosition;

    REQUIRE(simple_platformer::worldToGrid(32, {31.9F, 32.0F}) == GridPosition{0, 1});
    REQUIRE(simple_platformer::gridToWorld(32, {2, 3}) == glm::vec2{64.0F, 96.0F});
    REQUIRE(simple_platformer::cellAtFeet(32, {48.0F, 64.0F}) == GridPosition{1, 1});
    REQUIRE(simple_platformer::feetInCell(32, {1, 1}) == glm::vec2{48.0F, 64.0F});
    REQUIRE(
        simple_platformer::boxInCell(32, {1, 1}, {12.0F, 20.0F}).position ==
        glm::vec2{42.0F, 44.0F});
}

TEST_CASE("Cells and feet convert on tile boundaries", "[math][coordinates]")
{
    REQUIRE(
        simple_platformer::cellAtFeet(16, {24.0F, 32.0F}) == simple_platformer::GridPosition{1, 1});
    REQUIRE(simple_platformer::feetInCell(16, {1, 1}) == glm::vec2{24.0F, 32.0F});
}

TEST_CASE("A box covers the cells inside its edges, not the ones it only touches", "[math][aabb]")
{
    using simple_platformer::GridPosition;

    // Every edge on a boundary: exactly the one cell.
    const auto onBoundaries = simple_platformer::cellsCovered(16, {{16.0F, 16.0F}, {16.0F, 16.0F}});
    REQUIRE(onBoundaries.first == GridPosition{1, 1});
    REQUIRE(onBoundaries.last == GridPosition{1, 1});

    const auto straddling = simple_platformer::cellsCovered(16, {{8.0F, 8.0F}, {16.0F, 16.0F}});
    REQUIRE(straddling.first == GridPosition{0, 0});
    REQUIRE(straddling.last == GridPosition{1, 1});

    // A 12 by 20 body standing in a cell reaches into the cell above, and no further sideways.
    const auto standing = simple_platformer::cellsCovered(
        16, simple_platformer::boxInCell(16, {1, 1}, {12.0F, 20.0F}));
    REQUIRE(standing.first == GridPosition{1, 0});
    REQUIRE(standing.last == GridPosition{1, 1});
}
