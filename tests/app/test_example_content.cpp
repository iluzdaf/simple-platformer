#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "game/example_content.hpp"
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/world/level_validation.hpp"
#include "simple_platformer/world/world_simulation.hpp"

namespace
{
    struct PatrolObservation
    {
        simple_platformer::ActorId actor;
        glm::vec2 initialFeet;
        bool initialHeadingToSecond = true;
        float furthestDistanceSquared = 0.0F;
        bool changedEndpoint = false;
    };
}

TEST_CASE("Example level selection records the requested number", "[app][content]")
{
    for (const int requested : {1, 2})
    {
        const auto content = simple_platformer::makeExampleLevel(requested, 0);
        REQUIRE(content.number == requested);
    }

    REQUIRE_THROWS_AS(simple_platformer::makeExampleLevel(3, 0), std::invalid_argument);
}

TEST_CASE("Every NPC patrol in the supplied levels makes progress", "[app][content][patrol]")
{
    constexpr float FixedDeltaTime = 1.0F / 60.0F;
    constexpr int SimulationTicks = 1200;
    constexpr float MeaningfulDistance = 16.0F;

    for (int level = 1; level <= 2; ++level)
    {
        DYNAMIC_SECTION("Level " << level)
        {
            simple_platformer::ExampleLevel content = simple_platformer::makeExampleLevel(level, 0);

            simple_platformer::Actor player = simple_platformer::makeExamplePlayer(0);
            simple_platformer::placeFeetAt(player.body.bounds, content.playerSpawnFeet);
            const simple_platformer::ActorId playerId = content.world.addActor(player);
            content.world.setPlayer(playerId, content.playerSpawnFeet);
            simple_platformer::validateLevelActors(content.map, content.world, content.number);

            std::vector<PatrolObservation> patrols;
            for (const simple_platformer::Actor& actor : content.world.actors())
            {
                if (actor.patrol.has_value())
                {
                    patrols.push_back(
                        {actor.id,
                         simple_platformer::feetOf(actor.body.bounds),
                         actor.patrol->headingToSecond});
                }
            }
            REQUIRE_FALSE(patrols.empty());

            for (int tick = 0; tick < SimulationTicks; ++tick)
            {
                simple_platformer::updateWorldSimulation(
                    content.map, content.world, FixedDeltaTime);
                for (PatrolObservation& observation : patrols)
                {
                    const simple_platformer::Actor* actor =
                        content.world.findActor(observation.actor);
                    if (actor == nullptr || !actor->patrol.has_value())
                    {
                        FAIL("An NPC patrol disappeared during the simulation");
                    }
                    const simple_platformer::Patrol patrol =
                        actor->patrol.value_or(simple_platformer::Patrol{});

                    const glm::vec2 offset =
                        simple_platformer::feetOf(actor->body.bounds) - observation.initialFeet;
                    observation.furthestDistanceSquared =
                        std::max(observation.furthestDistanceSquared, glm::dot(offset, offset));
                    observation.changedEndpoint =
                        observation.changedEndpoint ||
                        patrol.headingToSecond != observation.initialHeadingToSecond;
                }
            }

            for (const PatrolObservation& observation : patrols)
            {
                INFO("Level: " << level);
                INFO("Actor: " << observation.actor.value);
                INFO("Furthest distance squared: " << observation.furthestDistanceSquared);
                const bool madeProgress =
                    observation.furthestDistanceSquared >= MeaningfulDistance * MeaningfulDistance;
                REQUIRE((observation.changedEndpoint || madeProgress));
            }
        }
    }
}
