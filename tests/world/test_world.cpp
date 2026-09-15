#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/world/world.hpp"

namespace
{
    simple_platformer::Actor makeActor()
    {
        simple_platformer::Actor actor;
        actor.body.bounds = {{8.0F, 8.0F}, {12.0F, 12.0F}};
        actor.platformerMovement = simple_platformer::PlatformerMovement{};
        return actor;
    }
}

TEST_CASE("World assigns stable monotonically increasing actor IDs", "[world][actor]")
{
    simple_platformer::World world;

    const simple_platformer::ActorId first = world.addActor(makeActor());
    const simple_platformer::ActorId second = world.addActor(makeActor());
    REQUIRE(first.value == 1);
    REQUIRE(second.value == 2);

    REQUIRE(world.removeActor(first));
    const simple_platformer::ActorId third = world.addActor(makeActor());

    REQUIRE(third.value == 3);
    REQUIRE(world.findActor(first) == nullptr);
    REQUIRE(world.findActor(second) != nullptr);
    REQUIRE(world.findActor(third) != nullptr);
}

TEST_CASE("Actor IDs are not vector indexes", "[world][actor]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId first = world.addActor(makeActor());
    const simple_platformer::ActorId second = world.addActor(makeActor());

    world.removeActor(first);

    REQUIRE(world.actors().size() == 1);
    REQUIRE(world.actors().front().id == second);
    REQUIRE(second.value != 0);
}

TEST_CASE("World rejects invalid actor composition", "[world][actor]")
{
    simple_platformer::World world;

    simple_platformer::Actor missingMovement = makeActor();
    missingMovement.platformerMovement.reset();
    REQUIRE_THROWS_AS(world.addActor(missingMovement), std::invalid_argument);

    simple_platformer::Actor assignedId = makeActor();
    assignedId.id = {42};
    REQUIRE_THROWS_AS(world.addActor(assignedId), std::invalid_argument);

    simple_platformer::Actor invalidHealth = makeActor();
    invalidHealth.health = {4, 3};
    REQUIRE_THROWS_AS(world.addActor(invalidHealth), std::invalid_argument);

    simple_platformer::Actor animatorWithoutSprite = makeActor();
    animatorWithoutSprite.animator = simple_platformer::Animator{};
    REQUIRE_THROWS_AS(world.addActor(animatorWithoutSprite), std::invalid_argument);

    simple_platformer::Actor twoAttacks = makeActor();
    twoAttacks.team = simple_platformer::Team::Player;
    twoAttacks.rangedWeapon = simple_platformer::RangedWeapon{};
    twoAttacks.bite = simple_platformer::BiteAttack{};
    REQUIRE_THROWS_AS(world.addActor(twoAttacks), std::invalid_argument);

    simple_platformer::Actor neutralAttacker = makeActor();
    neutralAttacker.rangedWeapon = simple_platformer::RangedWeapon{};
    REQUIRE_THROWS_AS(world.addActor(neutralAttacker), std::invalid_argument);

    simple_platformer::Actor twoMovementComponents = makeActor();
    twoMovementComponents.flyingMovement = simple_platformer::FlyingMovement{};
    REQUIRE_THROWS_AS(world.addActor(twoMovementComponents), std::invalid_argument);

    simple_platformer::Actor incompleteNpc = makeActor();
    incompleteNpc.brain = simple_platformer::NpcBrain{};
    REQUIRE_THROWS_AS(world.addActor(incompleteNpc), std::invalid_argument);
}

TEST_CASE("The world records the player and feet-based spawn", "[world][actor]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId player = world.addActor(makeActor());

    world.setPlayer(player, {40.0F, 48.0F});

    REQUIRE(world.playerId() == player);
    REQUIRE(world.playerSpawnFeet().x == 40.0F);
    REQUIRE(world.playerSpawnFeet().y == 48.0F);
    REQUIRE_THROWS_AS(world.setPlayer({999}, {0.0F, 0.0F}), std::invalid_argument);
}
