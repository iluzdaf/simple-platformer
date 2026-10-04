#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "content/game_catalogs.hpp"
#include "content/actor_definition.hpp"
#include "content/level_catalog.hpp"
#include "game/level_composition.hpp"
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/level_validation.hpp"
#include "support/add_player.hpp"

namespace
{
    constexpr const char* ShippedAtlas = "assets/textures/sprites.png";

    // A PNG's width and height, the big-endian words at bytes 16 and 20 of its header.
    glm::ivec2 pngSize(const char* path)
    {
        std::ifstream file(path, std::ios::binary);
        std::array<unsigned char, 24> header{};
        file.read(reinterpret_cast<char*>(header.data()), header.size());
        REQUIRE(file.gcount() == static_cast<std::streamsize>(header.size()));
        const auto word = [&header](std::size_t at)
        {
            return static_cast<int>(
                (static_cast<std::uint32_t>(header[at]) << 24U) |
                (static_cast<std::uint32_t>(header[at + 1]) << 16U) |
                (static_cast<std::uint32_t>(header[at + 2]) << 8U) |
                static_cast<std::uint32_t>(header[at + 3]));
        };
        return {word(16), word(20)};
    }
}

// Level composition and placement

TEST_CASE("Every catalog level can be composed", "[app][content]")
{
    const auto catalog = simple_platformer::loadLevelCatalog("assets/levels/levels.json");
    const auto catalogs =
        simple_platformer::loadGameCatalogs("assets/catalogs", pngSize(ShippedAtlas));

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
    const auto catalogs =
        simple_platformer::loadGameCatalogs("assets/catalogs", pngSize(ShippedAtlas));
    for (const simple_platformer::LevelCatalogEntry& entry : catalog.levels)
    {
        auto content = simple_platformer::composeGameLevel(catalog, entry.number, 0, catalogs);
        simple_platformer::Actor player = simple_platformer::composePlayer(catalogs, 0);
        simple_platformer::moveFeetTo(player.body.bounds, content.playerSpawnFeet);
        tests::addPlayer(content.world, player);

        REQUIRE_NOTHROW(
            simple_platformer::validateLevelActors(content.map, content.world, content.number));
    }
}

// Atlas bounds

TEST_CASE("Every catalog region lies inside the shipped atlas", "[app][content][atlas]")
{
    const glm::ivec2 atlas = pngSize(ShippedAtlas);
    REQUIRE_NOTHROW(simple_platformer::loadGameCatalogs("assets/catalogs", atlas));
}
