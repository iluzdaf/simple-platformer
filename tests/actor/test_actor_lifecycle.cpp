#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_lifecycle.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
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

TEST_CASE("Fatal damage begins a timed death", "[actor][lifecycle]")
{
    simple_platformer::World world;
    simple_platformer::Actor actor = makeActor(1);
    actor.intentions.direction.x = 1.0F;
    const simple_platformer::ActorId id = world.addActor(actor);
    simple_platformer::WorldRequests requests;
    requests.damage(id, 1);

    simple_platformer::updateActorLifecycle(world, requests, 0.1F);
    simple_platformer::applyWorldRequests(world, requests);

    simple_platformer::Actor& dying = tests::actor(world, id);
    REQUIRE(tests::health(dying).current == 0);
    REQUIRE(dying.life == simple_platformer::LifeState::Dying);
    REQUIRE(dying.deathTimeRemaining == 0.4F);
    REQUIRE(dying.lastDamageTimeSeconds == 0.0F);
    REQUIRE(dying.intentions.direction.x == 0.0F);
}

TEST_CASE("Dying actors cannot take further damage", "[actor][lifecycle]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId id = world.addActor(makeActor(1));
    simple_platformer::WorldRequests requests;
    requests.damage(id, 1);
    simple_platformer::updateActorLifecycle(world, requests, 0.1F);
    simple_platformer::applyWorldRequests(world, requests);

    requests.damage(id, 1);
    simple_platformer::updateActorLifecycle(world, requests, 0.1F);
    simple_platformer::applyWorldRequests(world, requests);

    simple_platformer::Actor& dying = tests::actor(world, id);
    REQUIRE(tests::health(dying).current == 0);
    REQUIRE(dying.deathTimeRemaining < 0.4F);
}

TEST_CASE("An NPC is removed after its death timer", "[actor][lifecycle]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId npc = world.addActor(makeActor(1));
    simple_platformer::WorldRequests requests;
    requests.damage(npc, 1);
    simple_platformer::updateActorLifecycle(world, requests, 0.1F);
    simple_platformer::applyWorldRequests(world, requests);

    simple_platformer::updateActorLifecycle(world, requests, 0.4F);
    simple_platformer::applyWorldRequests(world, requests);

    REQUIRE(world.findActor(npc) == nullptr);
}

TEST_CASE("The player respawns with restored runtime state", "[actor][lifecycle]")
{
    simple_platformer::World world;
    simple_platformer::Actor actor = makeActor(1);
    actor.health = simple_platformer::Health{1, 3};
    actor.body.velocity = {20.0F, 30.0F};
    tests::platformerMovement(actor).grounded = true;
    tests::platformerMovement(actor).coyoteRemaining = 0.1F;
    tests::platformerMovement(actor).jumpBufferRemaining = 0.1F;
    const simple_platformer::ActorId player = world.addActor(actor);
    world.setPlayer(player, {40.0F, 48.0F});
    simple_platformer::WorldRequests requests;
    requests.damage(player, 1);
    simple_platformer::updateActorLifecycle(world, requests, 0.1F);
    simple_platformer::applyWorldRequests(world, requests);

    // Hits queued while dying must be ignored before the player respawns.
    requests.damage(player, 1);
    simple_platformer::updateActorLifecycle(world, requests, 0.4F);
    simple_platformer::applyWorldRequests(world, requests);

    simple_platformer::Actor& respawned = tests::actor(world, player);
    REQUIRE(respawned.life == simple_platformer::LifeState::Alive);
    REQUIRE_FALSE(respawned.lastDamageTimeSeconds.has_value());
    REQUIRE(tests::health(respawned).current == 3);
    REQUIRE(simple_platformer::feetOf(respawned.body.bounds).x == 40.0F);
    REQUIRE(simple_platformer::feetOf(respawned.body.bounds).y == 48.0F);
    REQUIRE(respawned.body.velocity.x == 0.0F);
    REQUIRE(respawned.body.velocity.y == 0.0F);
    REQUIRE_FALSE(tests::platformerMovement(respawned).grounded);
    REQUIRE(tests::platformerMovement(respawned).coyoteRemaining == 0.0F);
    REQUIRE(tests::platformerMovement(respawned).jumpBufferRemaining == 0.0F);
}

TEST_CASE("Actor lifecycle rejects a negative time step", "[actor][lifecycle]")
{
    simple_platformer::World world;
    simple_platformer::WorldRequests requests;
    REQUIRE_THROWS_AS(
        simple_platformer::updateActorLifecycle(world, requests, -1.0F), std::invalid_argument);
}
