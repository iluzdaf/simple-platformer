#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/require_near.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/tile_map_builder.hpp"
#include "support/add_player.hpp"
#include "support/fixed_step.hpp"

using tests::actor;
using tests::brain;
using tests::rangedWeapon;

namespace
{
    // An actor the world can treat as the player.
    tests::ActorBuilder makePlayer(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet(feet)
            .walking()
            .onTeam(simple_platformer::Team::Player);
    }

    // The NPC these tests measure their maps against.
    tests::ActorBuilder makeNpc(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .atFeet(feet)
            .flying(20.0F)
            .onTeam(simple_platformer::Team::Enemy)
            .thinking({64.0F, 1.0F});
    }
}

TEST_CASE("NPC sight observes distance and solid tiles", "[npc][senses]")
{
    const simple_platformer::TileMap clear =
        tests::TileMapBuilder({".....", ".....", ".....", "#####"});
    const simple_platformer::TileMap blocked =
        tests::TileMapBuilder({".....", "..x..", ".....", "#####"})
            .where('x', tests::Tile().blocksSight());
    const simple_platformer::Aabb observer{{8.0F, 16.0F}, {12.0F, 12.0F}};
    const simple_platformer::Aabb target{{56.0F, 16.0F}, {12.0F, 12.0F}};

    REQUIRE(simple_platformer::canSeeTarget(clear, observer, target, {64.0F, 1.0F}));
    REQUIRE_FALSE(simple_platformer::canSeeTarget(blocked, observer, target, {64.0F, 1.0F}));
    REQUIRE_FALSE(simple_platformer::canSeeTarget(clear, observer, target, {32.0F, 1.0F}));
}

TEST_CASE("Sight-blocking cover hides whoever stands in it", "[npc][senses]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".....", "c....", "....."}).where('c', tests::Tile().blocksSight());
    const simple_platformer::Aabb inCover{{2.0F, 18.0F}, {12.0F, 12.0F}};
    const simple_platformer::Aabb inOpen{{50.0F, 18.0F}, {12.0F, 12.0F}};

    REQUIRE_FALSE(simple_platformer::canSeeTarget(map, inOpen, inCover, {64.0F, 1.0F}));
    REQUIRE(simple_platformer::canSeeTarget(map, inCover, inOpen, {64.0F, 1.0F}));
}

TEST_CASE("Actors in one patch of sight-blocking cover see each other", "[npc][senses]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".....", "ccc..", "....."}).where('c', tests::Tile().blocksSight());
    const simple_platformer::Aabb first{{2.0F, 18.0F}, {12.0F, 12.0F}};
    const simple_platformer::Aabb second{{34.0F, 18.0F}, {12.0F, 12.0F}};

    REQUIRE(simple_platformer::canSeeTarget(map, first, second, {64.0F, 1.0F}));
    REQUIRE(simple_platformer::canSeeTarget(map, second, first, {64.0F, 1.0F}));
}

TEST_CASE("Actors at the same position in sight-blocking cover see each other", "[npc][senses]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".....", "c....", "....."}).where('c', tests::Tile().blocksSight());
    const simple_platformer::Aabb inCover{{2.0F, 18.0F}, {12.0F, 12.0F}};

    REQUIRE(simple_platformer::canSeeTarget(map, inCover, inCover, {64.0F, 1.0F}));
}

TEST_CASE("Actors in separate patches of sight-blocking cover are hidden", "[npc][senses]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".....", "c.c..", "....."}).where('c', tests::Tile().blocksSight());
    const simple_platformer::Aabb first{{2.0F, 18.0F}, {12.0F, 12.0F}};
    const simple_platformer::Aabb second{{34.0F, 18.0F}, {12.0F, 12.0F}};

    REQUIRE_FALSE(simple_platformer::canSeeTarget(map, first, second, {64.0F, 1.0F}));
    REQUIRE_FALSE(simple_platformer::canSeeTarget(map, second, first, {64.0F, 1.0F}));
}

TEST_CASE("Only sight-blocking tiles block sight between actors in the open", "[npc][senses]")
{
    const simple_platformer::TileMap covered =
        tests::TileMapBuilder({"...", ".c.", "..."}).where('c', tests::Tile().blocksSight());
    const simple_platformer::TileMap windowed =
        tests::TileMapBuilder({"...", ".w.", "..."}).where('w', tests::Tile().blocksMovement());
    const simple_platformer::Aabb left{{2.0F, 18.0F}, {12.0F, 12.0F}};
    const simple_platformer::Aabb right{{34.0F, 18.0F}, {12.0F, 12.0F}};

    REQUIRE_FALSE(simple_platformer::canSeeTarget(covered, left, right, {64.0F, 1.0F}));
    REQUIRE_FALSE(simple_platformer::canSeeTarget(covered, right, left, {64.0F, 1.0F}));
    REQUIRE(simple_platformer::canSeeTarget(windowed, left, right, {64.0F, 1.0F}));
    REQUIRE(simple_platformer::canSeeTarget(windowed, right, left, {64.0F, 1.0F}));
}

TEST_CASE("NPC target memory expires and rejects a dead player", "[npc][senses]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"............", "............", "............", "############"});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = tests::addPlayer(world, makePlayer({38.0F, 28.0F}));
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({22.0F, 28.0F}));

    simple_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE(brain(world, npcId).targetVisible);

    simple_platformer::placeFeetAt(actor(world, playerId).body.bounds, {166.0F, 28.0F});
    simple_platformer::updateNpcSenses(map, world, 0.4F);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE_FALSE(brain(world, npcId).targetVisible);
    REQUIRE_NEAR(brain(world, npcId).targetMemoryRemaining, 0.6F);

    simple_platformer::updateNpcSenses(map, world, 0.7F);
    REQUIRE_FALSE(brain(world, npcId).target.has_value());

    simple_platformer::placeFeetAt(actor(world, playerId).body.bounds, {38.0F, 28.0F});
    simple_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE(brain(world, npcId).targetVisible);
    actor(world, playerId).life = simple_platformer::LifeState::Dying;
    simple_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE_FALSE(brain(world, npcId).target.has_value());
}

TEST_CASE("An NPC remembers where it heard a hidden player shoot", "[npc][senses]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "..c.....", "........"})
            .where('c', tests::Tile().blocksSight());
    simple_platformer::World world;
    const simple_platformer::ActorId playerId =
        tests::addPlayer(world, makePlayer({40.0F, 30.0F}).shooting());
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({8.0F, 30.0F}));
    const glm::vec2 shotFeet{40.0F, 30.0F};

    simple_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE_FALSE(brain(world, npcId).target.has_value());

    // The attack system stamps the shot after senses ran; the next update hears it.
    rangedWeapon(world, playerId).lastFiredTimeSeconds = world.simulationTimeSeconds();
    world.advanceSimulationTime(0.1F);
    simple_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE_FALSE(brain(world, npcId).targetVisible);
    REQUIRE(brain(world, npcId).lastSeenTargetFeet == shotFeet);
    REQUIRE_NEAR(brain(world, npcId).targetMemoryRemaining, 1.0F);

    // An older shot is not heard again, so moving away doesn't update the remembered spot.
    simple_platformer::placeFeetAt(actor(world, playerId).body.bounds, {118.0F, 30.0F});
    world.advanceSimulationTime(0.4F);
    simple_platformer::updateNpcSenses(map, world, 0.4F);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE(brain(world, npcId).lastSeenTargetFeet == shotFeet);
    REQUIRE_NEAR(brain(world, npcId).targetMemoryRemaining, 0.6F);
}

TEST_CASE("An NPC hears a shot through a wall", "[npc][senses]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({".....", "..x..", "....."}).where('x', tests::Tile().blocksSight());
    simple_platformer::World world;
    const simple_platformer::ActorId playerId =
        tests::addPlayer(world, makePlayer({56.0F, 30.0F}).shooting());
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({8.0F, 30.0F}));

    rangedWeapon(world, playerId).lastFiredTimeSeconds = world.simulationTimeSeconds();
    world.advanceSimulationTime(0.1F);
    simple_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE(brain(world, npcId).target == playerId);
    REQUIRE_FALSE(brain(world, npcId).targetVisible);
    REQUIRE(brain(world, npcId).lastSeenTargetFeet == glm::vec2{56.0F, 30.0F});
}

TEST_CASE("An NPC does not hear a shot beyond its notice distance", "[npc][senses]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "..........", ".........."});
    simple_platformer::World world;
    const simple_platformer::ActorId playerId =
        tests::addPlayer(world, makePlayer({104.0F, 30.0F}).shooting());
    const simple_platformer::ActorId npcId = world.addActor(makeNpc({8.0F, 30.0F}));

    rangedWeapon(world, playerId).lastFiredTimeSeconds = world.simulationTimeSeconds();
    world.advanceSimulationTime(0.1F);
    simple_platformer::updateNpcSenses(map, world, 0.1F);
    REQUIRE_FALSE(brain(world, npcId).target.has_value());
}

TEST_CASE("Whether any NPC sees the player is what the senses update decided", "[npc][senses]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"............", "...ccccccc..", "............"})
            .where('c', tests::Tile().blocksSight());
    simple_platformer::World world;
    tests::addPlayer(world, makePlayer({56.0F, 30.0F}));
    const auto sensed = [&]
    {
        simple_platformer::updateNpcSenses(map, world, tests::FixedStepSeconds);
        return simple_platformer::playerSeenByAnyNpc(world);
    };

    // Nobody looking.
    REQUIRE_FALSE(sensed());

    // An NPC outside the patch cannot see in.
    world.addActor(makeNpc({8.0F, 30.0F}));
    REQUIRE_FALSE(sensed());

    // One in the same patch but beyond its notice distance does not notice.
    world.addActor(makeNpc({152.0F, 30.0F}));
    REQUIRE_FALSE(sensed());

    // One in the same patch within notice distance sees the player.
    const simple_platformer::ActorId nearby = world.addActor(makeNpc({88.0F, 30.0F}));
    REQUIRE(sensed());

    // A dying NPC no longer looks, and the flag it set is cleared on the next update.
    actor(world, nearby).life = simple_platformer::LifeState::Dying;
    REQUIRE_FALSE(sensed());
}

TEST_CASE("NPC senses reject invalid timing and sensing ranges", "[npc][validation]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", "...", "###"});
    const simple_platformer::Aabb bounds{{16.0F, 16.0F}, {8.0F, 8.0F}};
    simple_platformer::World world;

    REQUIRE_THROWS_AS(
        simple_platformer::canSeeTarget(map, bounds, bounds, {-1.0F, 1.0F}), std::invalid_argument);
    REQUIRE_THROWS_AS(simple_platformer::updateNpcSenses(map, world, -0.1F), std::invalid_argument);
}

TEST_CASE("A brain's living target is the remembered actor while it is alive", "[npc][senses]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId playerId = tests::addPlayer(world, makePlayer({38.0F, 28.0F}));
    simple_platformer::NpcBrain brain;
    REQUIRE(simple_platformer::livingTarget(world, brain) == nullptr);

    brain.target = playerId;
    REQUIRE(simple_platformer::livingTarget(world, brain) == &tests::player(world));

    tests::player(world).life = simple_platformer::LifeState::Dying;
    REQUIRE(simple_platformer::livingTarget(world, brain) == nullptr);

    brain.target = simple_platformer::ActorId{999};
    REQUIRE(simple_platformer::livingTarget(world, brain) == nullptr);
}
