#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
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
    tests::ActorBuilder makePlayer(glm::vec2 feet)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F}).atFeet(feet).walking();
    }

    // A grounded walker at (24, 32) that notices within 32 pixels.
    simple_platformer::ActorId addWalkingNpc(simple_platformer::World& world)
    {
        const simple_platformer::ActorId npcId =
            world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                               .atFeet({24.0F, 32.0F})
                               .walking()
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

TEST_CASE("Same-run and notice-distance facts are independent", "[npc][facts]")
{
    struct ExpectedFacts
    {
        bool sameRun;
        bool withinNoticeDistance;
    };

    struct Scenario
    {
        std::string name;
        ExpectedFacts expected;
        std::string floor = "############";
        glm::vec2 targetFeet{56.0F, 32.0F};
        bool targetGrounded = true;
        bool rememberTarget = true;
        bool targetAlive = true;
    };

    const auto observeFacts = [](const Scenario& scenario)
    {
        const simple_platformer::TileMap map =
            tests::TileMapBuilder({"............", "............", scenario.floor});
        simple_platformer::World world;
        const auto targetId = world.addActor(makePlayer(scenario.targetFeet));
        tests::platformerMovement(actor(world, targetId)).grounded = scenario.targetGrounded;
        if (!scenario.targetAlive)
        {
            actor(world, targetId).life = simple_platformer::LifeState::Dying;
        }
        const auto npcId = addWalkingNpc(world);
        if (scenario.rememberTarget)
        {
            brain(world, npcId).target = targetId;
        }
        // These facts intentionally use current geometry, not visibility or last-known feet.
        brain(world, npcId).lastKnownTargetFeet = {24.0F, 32.0F};
        return factsOf(map, world, npcId);
    };

    constexpr ExpectedFacts SameRunAndWithinNotice{true, true};
    constexpr ExpectedFacts SameRunOnly{true, false};
    constexpr ExpectedFacts WithinNoticeOnly{false, true};
    constexpr ExpectedFacts Neither{false, false};
    std::vector<Scenario> scenarios{
        {"Same run at the inclusive notice boundary", SameRunAndWithinNotice}};

    Scenario beyondNotice{"Same run beyond notice distance", SameRunOnly};
    beyondNotice.targetFeet.x = 120.0F;
    scenarios.push_back(beyondNotice);

    Scenario nearbyGap{"Nearby across a gap", WithinNoticeOnly};
    nearbyGap.floor = "##.#########";
    scenarios.push_back(nearbyGap);

    Scenario distantGap{"Distant across a gap", Neither};
    distantGap.floor = "##.#########";
    distantGap.targetFeet.x = 120.0F;
    scenarios.push_back(distantGap);

    Scenario airborne{"Nearby but airborne", WithinNoticeOnly};
    airborne.targetGrounded = false;
    scenarios.push_back(airborne);

    Scenario forgotten{"No remembered target", Neither};
    forgotten.rememberTarget = false;
    scenarios.push_back(forgotten);

    Scenario dying{"A dead remembered target", Neither};
    dying.targetAlive = false;
    scenarios.push_back(dying);

    for (const Scenario& scenario : scenarios)
    {
        INFO(scenario.name);
        const auto facts = observeFacts(scenario);
        REQUIRE_FALSE(facts.targetVisible);
        REQUIRE(facts.targetOnSameRun == scenario.expected.sameRun);
        REQUIRE(facts.targetWithinNoticeDistance == scenario.expected.withinNoticeDistance);
    }
}

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
