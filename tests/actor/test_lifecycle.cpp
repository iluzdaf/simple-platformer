#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/actor/lifecycle.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"

namespace
{
    simple_platformer::Actor makeActor(int health = 3)
    {
        simple_platformer::Actor actor;
        actor.body.bounds = {{8.0F, 8.0F}, {12.0F, 12.0F}};
        actor.platformerMovement = simple_platformer::PlatformerMovement{};
        actor.health = simple_platformer::Health{health, health};
        return actor;
    }

    const simple_platformer::Health& healthOf(const simple_platformer::Actor& actor)
    {
        if (!actor.health.has_value())
        {
            throw std::logic_error("Test actor has no health");
        }
        return *actor.health;
    }

    const simple_platformer::PlatformerMovement& movementOf(const simple_platformer::Actor& actor)
    {
        if (!actor.platformerMovement.has_value())
        {
            throw std::logic_error("Test actor has no platformer movement");
        }
        return *actor.platformerMovement;
    }
}

TEST_CASE("Damage is deferred until lifecycle requests are applied", "[actor][lifecycle]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId id = world.addActor(makeActor());
    simple_platformer::WorldRequests requests;

    requests.damage(id, 1);
    const simple_platformer::Actor* undamaged = world.findActor(id);
    REQUIRE(undamaged != nullptr);
    REQUIRE(healthOf(*undamaged).current == 3);

    simple_platformer::updateLifeState(world, requests, 0.1F);

    const simple_platformer::Actor* damaged = world.findActor(id);
    REQUIRE(damaged != nullptr);
    REQUIRE(healthOf(*damaged).current == 2);
    REQUIRE(damaged->life == simple_platformer::LifeState::Alive);
    REQUIRE(requests.empty());
}

TEST_CASE("Fatal damage begins a timed death", "[actor][lifecycle]")
{
    simple_platformer::World world;
    simple_platformer::Actor actor = makeActor(1);
    actor.intentions.direction.x = 1.0F;
    const simple_platformer::ActorId id = world.addActor(actor);
    simple_platformer::WorldRequests requests;
    requests.damage(id, 1);

    simple_platformer::updateLifeState(world, requests, 0.1F);

    const simple_platformer::Actor* dying = world.findActor(id);
    REQUIRE(dying != nullptr);
    REQUIRE(healthOf(*dying).current == 0);
    REQUIRE(dying->life == simple_platformer::LifeState::Dying);
    REQUIRE(dying->deathTimeRemaining == 0.4F);
    REQUIRE(dying->intentions.direction.x == 0.0F);
}

TEST_CASE("Dying actors cannot take further damage", "[actor][lifecycle]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId id = world.addActor(makeActor(1));
    simple_platformer::WorldRequests requests;
    requests.damage(id, 1);
    simple_platformer::updateLifeState(world, requests, 0.1F);

    requests.damage(id, 1);
    simple_platformer::updateLifeState(world, requests, 0.1F);

    const simple_platformer::Actor* dying = world.findActor(id);
    REQUIRE(dying != nullptr);
    REQUIRE(healthOf(*dying).current == 0);
    REQUIRE(dying->deathTimeRemaining < 0.4F);
}

TEST_CASE("An NPC is removed after its death timer", "[actor][lifecycle]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId npc = world.addActor(makeActor(1));
    simple_platformer::WorldRequests requests;
    requests.damage(npc, 1);
    simple_platformer::updateLifeState(world, requests, 0.1F);

    simple_platformer::updateLifeState(world, requests, 0.4F);

    REQUIRE(world.findActor(npc) != nullptr);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(world.findActor(npc) == nullptr);
}

TEST_CASE("The player respawns with restored runtime state", "[actor][lifecycle]")
{
    simple_platformer::World world;
    simple_platformer::Actor actor = makeActor(1);
    actor.health = simple_platformer::Health{1, 3};
    actor.body.velocity = {20.0F, 30.0F};
    simple_platformer::PlatformerMovement movement;
    movement.grounded = true;
    movement.coyoteRemaining = 0.1F;
    movement.jumpBufferRemaining = 0.1F;
    actor.platformerMovement = movement;
    const simple_platformer::ActorId player = world.addActor(actor);
    world.setPlayer(player, {40.0F, 48.0F});
    simple_platformer::WorldRequests requests;
    requests.damage(player, 1);
    simple_platformer::updateLifeState(world, requests, 0.1F);

    simple_platformer::updateLifeState(world, requests, 0.4F);

    const simple_platformer::Actor* respawned = world.findActor(player);
    REQUIRE(respawned != nullptr);
    REQUIRE(respawned->life == simple_platformer::LifeState::Alive);
    REQUIRE(healthOf(*respawned).current == 3);
    REQUIRE(simple_platformer::feetOf(respawned->body.bounds).x == 40.0F);
    REQUIRE(simple_platformer::feetOf(respawned->body.bounds).y == 48.0F);
    REQUIRE(respawned->body.velocity.x == 0.0F);
    REQUIRE(respawned->body.velocity.y == 0.0F);
    REQUIRE_FALSE(movementOf(*respawned).grounded);
    REQUIRE(movementOf(*respawned).coyoteRemaining == 0.0F);
    REQUIRE(movementOf(*respawned).jumpBufferRemaining == 0.0F);
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

TEST_CASE("Invalid lifecycle requests and timing are rejected", "[actor][lifecycle]")
{
    simple_platformer::WorldRequests requests;
    REQUIRE_THROWS_AS(requests.damage({}, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(requests.damage({1}, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(requests.remove({}), std::invalid_argument);

    simple_platformer::World world;
    REQUIRE_THROWS_AS(
        simple_platformer::updateLifeState(world, requests, -1.0F), std::invalid_argument);
}
