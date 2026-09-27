#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "content/game_catalogs.hpp"
#include "content/level_catalog.hpp"
#include "content/npc_script_catalog.hpp"
#include "game/level_composition.hpp"
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/scripting/lua_npc_scripts.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/level_validation.hpp"
#include "support/add_player.hpp"

TEST_CASE("Every catalog level can be composed", "[app][content]")
{
    const auto catalog = simple_platformer::loadLevelCatalog("assets/levels/levels.json");
    const auto catalogs = simple_platformer::loadGameCatalogs("assets/catalogs");

    REQUIRE_FALSE(catalog.levels.empty());
    for (const simple_platformer::LevelCatalogEntry& entry : catalog.levels)
    {
        const auto content =
            simple_platformer::composeGameLevel(catalog, entry.number, 0, catalogs);
        REQUIRE(content.number == entry.number);

        const auto& levelExit = content.world.exit();
        if (!levelExit.has_value())
        {
            throw std::logic_error("A composed level must have an exit");
        }
        const simple_platformer::LevelExit& exit = levelExit.value();
        if (exit.nextLevel.has_value())
        {
            REQUIRE_NOTHROW(simple_platformer::levelPath(catalog, exit.nextLevel.value()));
        }
    }
}

TEST_CASE("Every catalog level has valid actor placement", "[app][content]")
{
    const auto catalog = simple_platformer::loadLevelCatalog("assets/levels/levels.json");
    const auto catalogs = simple_platformer::loadGameCatalogs("assets/catalogs");
    for (const simple_platformer::LevelCatalogEntry& entry : catalog.levels)
    {
        auto content = simple_platformer::composeGameLevel(catalog, entry.number, 0, catalogs);
        simple_platformer::Actor player = simple_platformer::composePlayer(catalogs, 0);
        simple_platformer::placeFeetAt(player.body.bounds, content.playerSpawnFeet);
        tests::addPlayer(content.world, player);

        REQUIRE_NOTHROW(
            simple_platformer::validateLevelActors(content.map, content.world, content.number));
    }
}

TEST_CASE("Every shipped Lua activity resolves", "[app][content][lua]")
{
    const simple_platformer::GameCatalogs catalogs =
        simple_platformer::loadGameCatalogs("assets/catalogs");
    simple_platformer::LuaNpcScripts scripts;

    REQUIRE_NOTHROW(
        simple_platformer::loadNpcActivityScripts(scripts, catalogs.machines, "assets/scripts"));
}
