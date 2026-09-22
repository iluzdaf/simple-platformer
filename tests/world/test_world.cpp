#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"

namespace
{
    simple_platformer::Actor makeActor()
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F}).at({8.0F, 8.0F}).walking();
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
}

TEST_CASE("World owns a validated simulation clock", "[world][time]")
{
    simple_platformer::World world;

    REQUIRE(world.simulationTimeSeconds() == 0.0F);

    world.advanceSimulationTime(0.25F);
    world.advanceSimulationTime(0.25F);

    REQUIRE(world.simulationTimeSeconds() == 0.5F);
    REQUIRE_THROWS_AS(world.advanceSimulationTime(-0.1F), std::invalid_argument);
    REQUIRE_THROWS_AS(
        world.advanceSimulationTime(std::numeric_limits<float>::infinity()), std::invalid_argument);

    simple_platformer::World overflowingWorld;
    overflowingWorld.advanceSimulationTime(std::numeric_limits<float>::max());
    REQUIRE_THROWS_AS(
        overflowingWorld.advanceSimulationTime(std::numeric_limits<float>::max()),
        std::overflow_error);
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

    simple_platformer::Actor invalidAnimator = makeActor();
    invalidAnimator.sprite = simple_platformer::Sprite{};
    invalidAnimator.animator = simple_platformer::Animator{};
    REQUIRE_THROWS_AS(world.addActor(invalidAnimator), std::invalid_argument);

    tests::animator(invalidAnimator)
        .animationSet.clips.push_back(
            {simple_platformer::AnimationName::Idle, {{{0.0F, 0.0F}, {1.0F, 1.0F}}}});
    tests::animator(invalidAnimator).elapsed = std::numeric_limits<float>::infinity();
    REQUIRE_THROWS_AS(world.addActor(invalidAnimator), std::invalid_argument);

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

    simple_platformer::Actor invalidBitingNpc = makeActor();
    invalidBitingNpc.brain = simple_platformer::NpcBrain{};
    tests::brain(invalidBitingNpc).state = simple_platformer::NpcState::Bite;
    invalidBitingNpc.senses = simple_platformer::NpcSenses{};
    invalidBitingNpc.pathFollower = simple_platformer::PathFollower{};
    REQUIRE_THROWS_AS(world.addActor(invalidBitingNpc), std::invalid_argument);

    simple_platformer::Actor futureDamage = makeActor();
    futureDamage.lastDamageTimeSeconds = 1.0F;
    REQUIRE_THROWS_AS(world.addActor(futureDamage), std::invalid_argument);
}

TEST_CASE("NPC composition does not require a bite attack", "[world][actor]")
{
    simple_platformer::World world;
    simple_platformer::Actor npc = makeActor();
    npc.brain = simple_platformer::NpcBrain{};
    npc.senses = simple_platformer::NpcSenses{};
    npc.pathFollower = simple_platformer::PathFollower{};

    const simple_platformer::ActorId npcId = world.addActor(npc);

    REQUIRE(world.findActor(npcId) != nullptr);
}

TEST_CASE("World adds and removes projectiles through its public interface", "[world][projectile]")
{
    simple_platformer::World world;
    simple_platformer::Projectile first;
    first.bounds = {{8.0F, 8.0F}, {4.0F, 2.0F}};
    first.sprite.size = {4.0F, 2.0F};
    simple_platformer::Projectile second = first;
    second.bounds.position = {16.0F, 8.0F};

    world.addProjectile(first);
    world.addProjectile(second);

    REQUIRE(world.removeProjectile(0));
    REQUIRE(world.projectiles().size() == 1);
    REQUIRE(world.projectiles().front().bounds.position.x == 16.0F);
    REQUIRE_FALSE(world.removeProjectile(1));
}

TEST_CASE("World validates and owns projectile bursts", "[world][projectile]")
{
    simple_platformer::World world;
    simple_platformer::ProjectileBurst burst;
    burst.center = {8.0F, 8.0F};
    burst.sprite.size = {4.0F, 2.0F};

    world.addProjectileBurst(burst);

    REQUIRE(world.projectileBursts().size() == 1);
    REQUIRE(world.removeProjectileBurst(0));
    REQUIRE(world.projectileBursts().empty());
    REQUIRE_FALSE(world.removeProjectileBurst(0));

    burst.direction = {0.0F, 0.0F};
    REQUIRE_THROWS_AS(world.addProjectileBurst(burst), std::invalid_argument);
    burst.direction = {1.0F, 0.0F};
    burst.lifetimeRemaining = burst.duration + 0.1F;
    REQUIRE_THROWS_AS(world.addProjectileBurst(burst), std::invalid_argument);
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
