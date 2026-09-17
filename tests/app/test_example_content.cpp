#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "game/example_content.hpp"
#include "game/example_items.hpp"
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/world/level_validation.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
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

TEST_CASE("Every NPC patrol in the supplied levels makes progress", "[app][content][patrol]")
{
    constexpr float FixedDeltaTime = 1.0F / 60.0F;
    constexpr int SimulationTicks = 1200;
    constexpr float MeaningfulDistance = 16.0F;

    for (int level = 1; level <= 2; ++level)
    {
        DYNAMIC_SECTION("Level " << level)
        {
            const simple_platformer::TileMap map = simple_platformer::makeExampleLevel(level);
            simple_platformer::World world(simple_platformer::makeExampleItems(0));

            simple_platformer::Actor player = simple_platformer::makeExamplePlayer(0);
            const glm::vec2 playerSpawn = simple_platformer::feetOf(player.body.bounds);
            const simple_platformer::ActorId playerId = world.addActor(player);
            world.setPlayer(playerId, playerSpawn);
            simple_platformer::populateExampleLevel(world, level, 0);
            simple_platformer::validateLevelActors(map, world, level);

            std::vector<PatrolObservation> patrols;
            for (const simple_platformer::Actor& actor : world.actors())
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
                simple_platformer::updateWorldSimulation(map, world, FixedDeltaTime);
                for (PatrolObservation& observation : patrols)
                {
                    const simple_platformer::Actor* actor = world.findActor(observation.actor);
                    if (actor == nullptr || !actor->patrol.has_value())
                    {
                        FAIL("An NPC patrol disappeared during the simulation");
                    }

                    const glm::vec2 offset =
                        simple_platformer::feetOf(actor->body.bounds) - observation.initialFeet;
                    observation.furthestDistanceSquared =
                        std::max(observation.furthestDistanceSquared, glm::dot(offset, offset));
                    observation.changedEndpoint =
                        observation.changedEndpoint ||
                        actor->patrol->headingToSecond != observation.initialHeadingToSecond;
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
