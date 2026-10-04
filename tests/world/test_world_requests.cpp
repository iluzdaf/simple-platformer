#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_lifecycle.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"

namespace
{
    simple_platformer::Actor makeActor(int health = 3)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .at({8.0F, 8.0F})
            .platforming()
            .withHealth(health, health);
    }
}

TEST_CASE("Explicit removals are deferred until world requests are applied", "[world][requests]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId id = world.addActor(makeActor());
    simple_platformer::WorldRequests requests;
    requests.remove(id);

    REQUIRE(world.findActor(id) != nullptr);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(world.findActor(id) == nullptr);
    REQUIRE(requests.empty());
}

TEST_CASE("Invalid world requests are rejected", "[world][requests]")
{
    simple_platformer::WorldRequests requests;
    REQUIRE_THROWS_AS(requests.damage({}, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(requests.damage({1}, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(requests.remove({}), std::invalid_argument);
}

TEST_CASE("World requests leave damage and death timers to actor lifecycle", "[world][requests]")
{
    simple_platformer::World world;
    const auto id = world.addActor(makeActor(1));
    simple_platformer::WorldRequests requests;
    requests.damage(id, 1);

    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::health(world, id).current == 1);
    REQUIRE(tests::actor(world, id).life == simple_platformer::LifeState::Alive);
    REQUIRE_FALSE(requests.empty());

    simple_platformer::updateActorLifecycle(world, requests, 0.1F);
    REQUIRE(tests::actor(world, id).deathTimeRemaining == simple_platformer::ActorDeathSeconds);
    REQUIRE(requests.empty());

    simple_platformer::updateActorLifecycle(world, requests, 0.1F);
    const float remaining = tests::actor(world, id).deathTimeRemaining;
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::actor(world, id).deathTimeRemaining == remaining);
}
