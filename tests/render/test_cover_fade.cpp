#include <catch2/catch_test_macros.hpp>

#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/render/cover_fade.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/require_near.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    constexpr float QuarterFade = simple_platformer::CoverFadeSeconds / 4.0F;

    simple_platformer::TileMap patchMap()
    {
        return tests::TileMapBuilder({"........", "...cc...", "........"})
            .where('c', tests::Tile().blocksSight());
    }

    simple_platformer::ActorId addPlayerIn(
        simple_platformer::World& world,
        simple_platformer::GridPosition cell)
    {
        return tests::addPlayer(
            world, tests::ActorBuilder::sized({12.0F, 12.0F}).inCell(cell).walking());
    }

    simple_platformer::ActorId addNpcIn(
        simple_platformer::World& world,
        simple_platformer::GridPosition cell)
    {
        return world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F}).inCell(cell).flying(0.0F));
    }

    void movePlayerTo(simple_platformer::World& world, simple_platformer::GridPosition cell)
    {
        tests::player(world).body.bounds =
            simple_platformer::boxInCell(tests::TileSize, cell, {12.0F, 12.0F});
    }

    float shown(simple_platformer::World& world, simple_platformer::ActorId id)
    {
        const std::optional<float>& visibility = tests::actor(world, id).screenVisibility;
        REQUIRE(visibility.has_value());
        return visibility.value_or(-1.0F);
    }
}

TEST_CASE("A newly placed NPC is shown at its target at once", "[render][cover-fade]")
{
    const simple_platformer::TileMap map = patchMap();
    simple_platformer::World world;
    addPlayerIn(world, {0, 1});
    const simple_platformer::ActorId inCover = addNpcIn(world, {4, 1});
    const simple_platformer::ActorId inOpen = addNpcIn(world, {7, 1});

    simple_platformer::updateCoverFades(map, world, QuarterFade);

    REQUIRE(shown(world, inCover) == 0.0F);
    REQUIRE(shown(world, inOpen) == 1.0F);
}

TEST_CASE(
    "An NPC fades in over the fade time when the player joins its patch",
    "[render][cover-fade]")
{
    const simple_platformer::TileMap map = patchMap();
    simple_platformer::World world;
    addPlayerIn(world, {0, 1});
    const simple_platformer::ActorId npc = addNpcIn(world, {4, 1});
    simple_platformer::updateCoverFades(map, world, QuarterFade);

    movePlayerTo(world, {3, 1});
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE_NEAR(shown(world, npc), 0.25F);

    simple_platformer::updateCoverFades(map, world, QuarterFade);
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE_NEAR(shown(world, npc), 1.0F);

    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE(shown(world, npc) == 1.0F);
}

TEST_CASE("An NPC fades out again when the player leaves its patch", "[render][cover-fade]")
{
    const simple_platformer::TileMap map = patchMap();
    simple_platformer::World world;
    addPlayerIn(world, {3, 1});
    const simple_platformer::ActorId npc = addNpcIn(world, {4, 1});
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE(shown(world, npc) == 1.0F);

    movePlayerTo(world, {0, 1});
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE_NEAR(shown(world, npc), 0.75F);
}

TEST_CASE("Pickups fade the same way as NPCs", "[render][cover-fade]")
{
    const simple_platformer::TileMap map = patchMap();
    simple_platformer::ItemDefinition coin;
    coin.id = 1;
    coin.name = "coin";
    simple_platformer::World world({coin});
    addPlayerIn(world, {0, 1});
    world.addPickup(
        {{simple_platformer::boxInCell(tests::TileSize, {3, 1}, {8.0F, 8.0F})}, {1, 1}});
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE(world.pickups().front().screenVisibility == 0.0F);

    movePlayerTo(world, {4, 1});
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE(world.pickups().front().screenVisibility.has_value());
    REQUIRE_NEAR(world.pickups().front().screenVisibility.value_or(-1.0F), 0.25F);
}

TEST_CASE("A player alone in cover is shown concealed", "[render][cover-fade]")
{
    const simple_platformer::TileMap map = patchMap();
    simple_platformer::World world;
    const simple_platformer::ActorId player = addPlayerIn(world, {3, 1});

    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE(shown(world, player) == 0.0F);

    movePlayerTo(world, {0, 1});
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE_NEAR(shown(world, player), 0.25F);
}

TEST_CASE("An NPC that can see the player exposes them", "[render][cover-fade]")
{
    const simple_platformer::TileMap map = patchMap();
    simple_platformer::World world;
    const simple_platformer::ActorId player = addPlayerIn(world, {3, 1});
    world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                       .inCell({4, 1})
                       .flying(0.0F)
                       .thinking({64.0F, 1.0F}));

    simple_platformer::updateCoverFades(map, world, QuarterFade);

    REQUIRE(shown(world, player) == 1.0F);
}

TEST_CASE("Firing exposes a hidden player for the reveal window", "[render][cover-fade]")
{
    const simple_platformer::TileMap map = patchMap();
    simple_platformer::World world;
    const simple_platformer::ActorId player = tests::addPlayer(
        world,
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .inCell({3, 1})
            .walking()
            .onTeam(simple_platformer::Team::Player)
            .shooting());
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE(shown(world, player) == 0.0F);

    tests::rangedWeapon(world, player).lastFiredTimeSeconds = world.simulationTimeSeconds();
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE_NEAR(shown(world, player), 0.25F);

    world.advanceSimulationTime(simple_platformer::ShotRevealSeconds * 0.5F);
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE_NEAR(shown(world, player), 0.5F);

    world.advanceSimulationTime(simple_platformer::ShotRevealSeconds);
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE_NEAR(shown(world, player), 0.25F);
}
