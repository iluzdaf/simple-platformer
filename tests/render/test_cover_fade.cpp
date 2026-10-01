#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/render/cover_fade.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/fixed_step.hpp"
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
        simple_platformer::Cell cell)
    {
        return tests::addPlayer(
            world, tests::ActorBuilder::sized({12.0F, 12.0F}).inCell(cell).platforming());
    }

    simple_platformer::ActorId addNpcIn(
        simple_platformer::World& world,
        simple_platformer::Cell cell)
    {
        return world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F}).inCell(cell).flying(0.0F));
    }

    void movePlayerTo(simple_platformer::World& world, simple_platformer::Cell cell)
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

    // What the screen first shows of an NPC with these bounds, to a player standing in the
    // cell or, without one, by cover alone. A first update adopts its target at once.
    float firstShown(
        const simple_platformer::TileMap& map,
        const simple_platformer::Aabb& npcBounds,
        std::optional<simple_platformer::Cell> playerCell)
    {
        simple_platformer::World world;
        if (playerCell.has_value())
        {
            addPlayerIn(world, *playerCell);
        }
        const simple_platformer::ActorId npcId =
            world.addActor(tests::ActorBuilder::sized(npcBounds.size)
                               .atFeet(simple_platformer::feetOf(npcBounds))
                               .flying(0.0F));
        simple_platformer::updateCoverFades(map, world, QuarterFade);
        return shown(world, npcId);
    }

    simple_platformer::Aabb boxIn(simple_platformer::Cell cell)
    {
        return simple_platformer::boxInCell(tests::TileSize, cell, {12.0F, 12.0F});
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

    REQUIRE_NEAR(shown(world, inCover), 0.0F);
    REQUIRE_NEAR(shown(world, inOpen), 1.0F);
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
    REQUIRE_NEAR(shown(world, npc), 1.0F);
}

TEST_CASE("An NPC fades out again when the player leaves its patch", "[render][cover-fade]")
{
    const simple_platformer::TileMap map = patchMap();
    simple_platformer::World world;
    addPlayerIn(world, {3, 1});
    const simple_platformer::ActorId npc = addNpcIn(world, {4, 1});
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE_NEAR(shown(world, npc), 1.0F);

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
    REQUIRE_NEAR(shown(world, player), 0.0F);

    movePlayerTo(world, {0, 1});
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE_NEAR(shown(world, player), 0.25F);
}

TEST_CASE("An NPC that saw the player this update exposes them", "[render][cover-fade]")
{
    const simple_platformer::TileMap map = patchMap();
    simple_platformer::World world;
    const simple_platformer::ActorId player = addPlayerIn(world, {3, 1});
    tests::player(world).team = simple_platformer::Team::Player;
    world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                       .inCell({4, 1})
                       .flying(0.0F)
                       .onTeam(simple_platformer::Team::Enemy)
                       .thinking({64.0F, 1.0F}));

    // The screen reads what the senses update decided, in the order the simulation runs.
    simple_platformer::updateNpcSenses(map, world, tests::FixedStepSeconds);
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE_NEAR(shown(world, player), 1.0F);

    // Move the NPC out of sight: the next senses update withdraws the exposure.
    world.actors().back().body.bounds.topLeft.x = 0.0F;
    simple_platformer::updateNpcSenses(map, world, tests::FixedStepSeconds);
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE(shown(world, player) < 1.0F);
}

TEST_CASE("Firing exposes a hidden player for the reveal window", "[render][cover-fade]")
{
    const simple_platformer::TileMap map = patchMap();
    simple_platformer::World world;
    const simple_platformer::ActorId player = tests::addPlayer(
        world,
        tests::ActorBuilder::sized({12.0F, 12.0F})
            .inCell({3, 1})
            .platforming()
            .onTeam(simple_platformer::Team::Player)
            .shooting());
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE_NEAR(shown(world, player), 0.0F);

    simple_platformer::WorldRequests requests;
    tests::actor(world, player).intentions.primaryAttackPressed = true;
    tests::actor(world, player).intentions.aimDirection = {1.0F, 0.0F};
    simple_platformer::updateAttacks(world, requests, 0.0F);
    tests::actor(world, player).intentions.primaryAttackPressed = false;
    REQUIRE(world.takeNoises().size() == 1); // Consuming the noise does not end the reveal.
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE_NEAR(shown(world, player), 0.25F);
    // Rendering (including paused frames) never ages a simulation-clock stamp.
    REQUIRE_NEAR(
        world.secondsSince(tests::rangedWeapon(world, player).lastFiredTimeSeconds).value_or(-1.0F),
        0.0F);

    world.advanceSimulationTime(simple_platformer::ShotRevealSeconds * 0.5F);
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE_NEAR(shown(world, player), 0.5F);

    world.advanceSimulationTime(simple_platformer::ShotRevealSeconds * 0.5F);
    REQUIRE_NEAR(
        world.secondsSince(tests::rangedWeapon(world, player).lastFiredTimeSeconds).value_or(-1.0F),
        simple_platformer::ShotRevealSeconds);
    simple_platformer::updateCoverFades(map, world, QuarterFade);
    REQUIRE_NEAR(shown(world, player), 0.25F);
}

TEST_CASE("Cover fades reject a negative step", "[render][cover]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"..", "##"});
    simple_platformer::World world;
    REQUIRE_THROWS_AS(
        simple_platformer::updateCoverFades(map, world, -0.1F), std::invalid_argument);
}

TEST_CASE(
    "Without a viewer, the share of a body in cover decides what is shown",
    "[render][cover-fade]")
{
    // One row of cover, two tiles wide.
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"....", ".cc.", "...."}).where('c', tests::Tile().blocksSight());
    const auto shownAt = [&map](glm::vec2 topLeft)
    { return firstShown(map, {topLeft, {16.0F, 16.0F}}, std::nullopt); };

    // Out of cover, and up to half in it, a body is shown fully.
    REQUIRE(shownAt({0.0F, 16.0F}) == 1.0F);
    REQUIRE(shownAt({8.0F, 16.0F}) == 1.0F);
    // Five eighths in cover is halfway between the thresholds.
    REQUIRE_NEAR(shownAt({10.0F, 16.0F}), 0.5F);
    // From three quarters in cover it is hidden.
    REQUIRE(shownAt({12.0F, 16.0F}) == 0.0F);
    REQUIRE(shownAt({20.0F, 16.0F}) == 0.0F);
    // Over the cover's corner, 12 by 14 of its 16 by 16 pixels are in cover.
    REQUIRE_NEAR(shownAt({12.0F, 18.0F}), 0.375F);
}

TEST_CASE("A player in cover sees its own patch fully but not another", "[render][cover-fade]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "...ccc.c", "........"})
            .where('c', tests::Tile().blocksSight());

    REQUIRE(firstShown(map, boxIn({5, 1}), simple_platformer::Cell{3, 1}) == 1.0F);
    REQUIRE(firstShown(map, boxIn({7, 1}), simple_platformer::Cell{3, 1}) == 0.0F);
}

TEST_CASE("Cover between the player and an NPC hides nothing", "[render][cover-fade]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "...cc...", "........"})
            .where('c', tests::Tile().blocksSight());

    // Only standing in cover hides.
    REQUIRE(firstShown(map, boxIn({7, 1}), simple_platformer::Cell{0, 1}) == 1.0F);
}

TEST_CASE("A wall hides nothing standing in the open", "[render][cover-fade]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".....", "..w..", "....."})
            .where('w', tests::Tile().blocksMovement().blocksSight());

    REQUIRE(firstShown(map, boxIn({4, 1}), simple_platformer::Cell{0, 1}) == 1.0F);
}
