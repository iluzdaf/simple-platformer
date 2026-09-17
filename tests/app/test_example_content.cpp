#include <catch2/catch_test_macros.hpp>

#include "game/example_content.hpp"
#include "game/example_level_catalog.hpp"
#include "simple_platformer/world/level_exit.hpp"

TEST_CASE("Every level in the example catalog can be composed", "[app][content]")
{
    const auto catalog = simple_platformer::loadExampleLevelCatalog();

    REQUIRE_FALSE(catalog.levels.empty());
    for (const simple_platformer::ExampleLevelEntry& entry : catalog.levels)
    {
        const auto content = simple_platformer::makeExampleLevel(catalog, entry.number, 0);
        REQUIRE(content.number == entry.number);

        const auto& levelExit = content.world.exit();
        REQUIRE(levelExit.has_value());
        const simple_platformer::LevelExit exit =
            levelExit.value_or(simple_platformer::LevelExit{});
        if (exit.nextLevel.has_value())
        {
            REQUIRE_NOTHROW(
                simple_platformer::exampleLevelPath(catalog, exit.nextLevel.value_or(0)));
        }
    }
}
