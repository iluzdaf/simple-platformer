#include <catch2/catch_test_macros.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_facts.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/tile_map_builder.hpp"

using tests::actor;
using tests::brain;

namespace
{
    // A grounded walker at (24, 32) that notices within 32 pixels.
    simple_platformer::ActorId addWalkingNpc(simple_platformer::World& world)
    {
        const simple_platformer::ActorId npcId =
            world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                               .atFeet({24.0F, 32.0F})
                               .platforming()
                               .thinking({32.0F, 1.0F}));
        tests::platformerMovement(actor(world, npcId)).grounded = true;
        return npcId;
    }

    simple_platformer::NpcFacts factsOf(
        const simple_platformer::TileMap& map,
        simple_platformer::World& world,
        simple_platformer::ActorId npcId)
    {
        const simple_platformer::NpcBrain& npcBrain = brain(world, npcId);
        return simple_platformer::gatherNpcFacts(
            map,
            actor(world, npcId),
            npcBrain,
            tests::perception(world, npcId),
            simple_platformer::livingTarget(world, npcBrain),
            0.0F);
    }
}

// Ground runs and notice distance

TEST_CASE("Ground-run membership and notice distance are checked separately", "[npc][facts]")
{
    simple_platformer::TileMap map =
        tests::TileMapBuilder({"............", "............", "############"});
    simple_platformer::World world;
    const auto targetId = world.addActor(
        tests::ActorBuilder::sized({12.0F, 12.0F}).atFeet({56.0F, 32.0F}).platforming());
    tests::platformerMovement(actor(world, targetId)).grounded = true;
    const auto npcId = addWalkingNpc(world);
    brain(world, npcId).target = targetId;
    // The facts use the target's current position even when it is unseen and remembered elsewhere.
    brain(world, npcId).lastKnownTargetFeet = {24.0F, 32.0F};

    SECTION("A target on the same floor at the notice boundary satisfies both facts")
    {
        const auto facts = factsOf(map, world, npcId);
        REQUIRE(facts.targetOnSameRun);
        REQUIRE(facts.targetWithinNoticeDistance);
    }
    SECTION("A distant target on the same floor satisfies only the ground-run fact")
    {
        simple_platformer::moveFeetTo(actor(world, targetId).body.bounds, {120.0F, 32.0F});
        const auto facts = factsOf(map, world, npcId);
        REQUIRE(facts.targetOnSameRun);
        REQUIRE_FALSE(facts.targetWithinNoticeDistance);
    }
    SECTION("A nearby target across a gap satisfies only the notice-distance fact")
    {
        map = tests::TileMapBuilder({"............", "............", "##.#########"});
        const auto facts = factsOf(map, world, npcId);
        REQUIRE_FALSE(facts.targetOnSameRun);
        REQUIRE(facts.targetWithinNoticeDistance);
    }
    SECTION("A distant target across a gap satisfies neither fact")
    {
        map = tests::TileMapBuilder({"............", "............", "##.#########"});
        simple_platformer::moveFeetTo(actor(world, targetId).body.bounds, {120.0F, 32.0F});
        const auto facts = factsOf(map, world, npcId);
        REQUIRE_FALSE(facts.targetOnSameRun);
        REQUIRE_FALSE(facts.targetWithinNoticeDistance);
    }
    SECTION("A nearby airborne target satisfies only the notice-distance fact")
    {
        tests::platformerMovement(actor(world, targetId)).grounded = false;
        const auto facts = factsOf(map, world, npcId);
        REQUIRE_FALSE(facts.targetOnSameRun);
        REQUIRE(facts.targetWithinNoticeDistance);
    }
    SECTION("An NPC without a remembered target satisfies neither fact")
    {
        brain(world, npcId).target.reset();
        const auto facts = factsOf(map, world, npcId);
        REQUIRE_FALSE(facts.targetOnSameRun);
        REQUIRE_FALSE(facts.targetWithinNoticeDistance);
    }
    SECTION("A dying remembered target satisfies neither fact")
    {
        actor(world, targetId).life = simple_platformer::LifeState::Dying;
        const auto facts = factsOf(map, world, npcId);
        REQUIRE_FALSE(facts.targetOnSameRun);
        REQUIRE_FALSE(facts.targetWithinNoticeDistance);
    }

    REQUIRE_FALSE(factsOf(map, world, npcId).targetVisible);
}

// Perception and movement facts

TEST_CASE("Heard landings and blocked walking are facts", "[npc][facts]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"............", "............", "############"});
    simple_platformer::World world;
    const auto npcId = addWalkingNpc(world);
    REQUIRE_FALSE(factsOf(map, world, npcId).heardLanding);
    REQUIRE_FALSE(factsOf(map, world, npcId).movementBlocked);

    tests::perception(world, npcId).heardLanding = true;
    tests::platformerMovement(actor(world, npcId)).blocked = true;
    REQUIRE(factsOf(map, world, npcId).heardLanding);
    REQUIRE(factsOf(map, world, npcId).movementBlocked);
}
