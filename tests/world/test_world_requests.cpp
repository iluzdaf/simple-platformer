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

TEST_CASE("Damage is deferred until world requests are applied", "[actor][lifecycle]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId id = world.addActor(makeActor());
    simple_platformer::WorldRequests requests;

    requests.damage(id, 1);
    simple_platformer::Actor& undamaged = tests::actor(world, id);
    REQUIRE(tests::health(undamaged).current == 3);

    simple_platformer::applyWorldRequests(world, requests);

    simple_platformer::Actor& damaged = tests::actor(world, id);
    REQUIRE(tests::health(damaged).current == 2);
    REQUIRE(damaged.life == simple_platformer::LifeState::Alive);
    REQUIRE(requests.empty());
}

TEST_CASE("Applied damage records the current simulation time", "[actor][lifecycle]")
{
    simple_platformer::World world;
    world.advanceSimulationTime(2.0F);
    const simple_platformer::ActorId id = world.addActor(makeActor());
    simple_platformer::WorldRequests requests;
    requests.damage(id, 1);

    simple_platformer::applyWorldRequests(world, requests);

    simple_platformer::Actor& damaged = tests::actor(world, id);
    REQUIRE(damaged.lastDamageTimeSeconds == 2.0F);
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

TEST_CASE("Untimed requests apply damage without advancing death timers", "[world][requests]")
{
    simple_platformer::World world;
    const auto id = world.addActor(makeActor(1));
    simple_platformer::WorldRequests requests;
    requests.damage(id, 1);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::actor(world, id).deathTimeRemaining == simple_platformer::ActorDeathSeconds);

    simple_platformer::updateActorLifecycle(world, requests, 0.1F);
    simple_platformer::applyWorldRequests(world, requests);
    const float remaining = tests::actor(world, id).deathTimeRemaining;
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::actor(world, id).deathTimeRemaining == remaining);
}
