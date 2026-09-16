#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <optional>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "graphics/display_viewport.hpp"

namespace
{
    using Catch::Matchers::WithinAbs;

    simple_platformer::DisplayViewport required(
        const std::optional<simple_platformer::DisplayViewport>& viewport)
    {
        if (!viewport.has_value())
        {
            throw std::logic_error("Test display viewport was not created");
        }
        return *viewport;
    }

    glm::vec2 required(const std::optional<glm::vec2>& position)
    {
        if (!position.has_value())
        {
            throw std::logic_error("Test cursor was outside the game viewport");
        }
        return *position;
    }
}

TEST_CASE("The display viewport uses the largest integer scale", "[app][viewport]")
{
    const simple_platformer::DisplayViewport viewport =
        required(simple_platformer::makeDisplayViewport({1000, 600}));

    REQUIRE(viewport.scale == 3);
    REQUIRE(viewport.size == glm::ivec2{960, 540});
    REQUIRE(viewport.topLeftMargin == glm::ivec2{20, 30});
    REQUIRE(viewport.bottomMargin == 30);
}

TEST_CASE("Odd letterbox space preserves both vertical origins", "[app][viewport]")
{
    const simple_platformer::DisplayViewport viewport =
        required(simple_platformer::makeDisplayViewport({1001, 601}));

    REQUIRE(viewport.topLeftMargin == glm::ivec2{20, 31});
    REQUIRE(viewport.bottomMargin == 30);
}

TEST_CASE("Window cursor positions account for high DPI and letterboxing", "[app][viewport]")
{
    const glm::vec2 internal =
        required(simple_platformer::windowToInternal({250.0F, 150.0F}, {500, 300}, {1000, 600}));

    REQUIRE_THAT(internal.x, WithinAbs(160.0F, 0.0001F));
    REQUIRE_THAT(internal.y, WithinAbs(90.0F, 0.0001F));
}

TEST_CASE("Cursor positions in the letterbox are rejected", "[app][viewport]")
{
    REQUIRE_FALSE(
        simple_platformer::windowToInternal({0.0F, 150.0F}, {500, 300}, {1000, 600}).has_value());
    REQUIRE_FALSE(simple_platformer::makeDisplayViewport({319, 179}).has_value());
}
