#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <optional>
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

TEST_CASE("World measures how long ago a stamp on its clock was", "[world][time]")
{
    simple_platformer::World world;
    REQUIRE_FALSE(world.secondsSince(std::nullopt).has_value());
    REQUIRE(world.secondsSince(0.0F) == 0.0F);

    world.advanceSimulationTime(0.5F);
    REQUIRE(world.secondsSince(0.25F) == 0.25F);
    REQUIRE_FALSE(world.secondsSince(std::nullopt).has_value());
    // A stamp cannot come from before the world began or from its future.
    REQUIRE_THROWS_AS(world.secondsSince(-0.1F), std::invalid_argument);
    REQUIRE_THROWS_AS(world.secondsSince(0.75F), std::invalid_argument);
}

TEST_CASE("World rejects invalid actor composition", "[world][actor]")
{
    simple_platformer::World world;
    simple_platformer::Actor actor = makeActor();

    SECTION("No movement component")
    {
        actor.platformerMovement.reset();
    }
    SECTION("Two movement components")
    {
        actor.flyingMovement = simple_platformer::FlyingMovement{};
    }
    SECTION("An id the world did not assign")
    {
        actor.id = {42};
    }
    SECTION("Health above its maximum")
    {
        actor.health = {4, 3};
    }
    SECTION("An animator without a sprite")
    {
        actor.animator = simple_platformer::Animator{};
    }
    SECTION("An animator with no clips")
    {
        actor.sprite = simple_platformer::Sprite{};
        actor.animator = simple_platformer::Animator{};
    }
    SECTION("An animator with infinite elapsed time")
    {
        actor.sprite = simple_platformer::Sprite{};
        actor.animator = simple_platformer::Animator{};
        tests::animator(actor).animationSet.clips.push_back(
            {simple_platformer::AnimationName::Idle, {{{0.0F, 0.0F}, {1.0F, 1.0F}}}});
        tests::animator(actor).elapsed = std::numeric_limits<float>::infinity();
    }
    SECTION("Two attacks")
    {
        actor.team = simple_platformer::Team::Player;
        actor.rangedWeapon = simple_platformer::RangedWeapon{};
        actor.bite = simple_platformer::BiteAttack{};
    }
    SECTION("A neutral attacker")
    {
        actor.rangedWeapon = simple_platformer::RangedWeapon{};
    }
    SECTION("A brain without senses or a path follower")
    {
        actor.brain = simple_platformer::NpcBrain{};
    }
    SECTION("Damage taken in the future")
    {
        actor.lastDamageTimeSeconds = 1.0F;
    }

    REQUIRE_THROWS_AS(world.addActor(actor), std::invalid_argument);
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
