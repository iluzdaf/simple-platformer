#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/actor/lifecycle.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/combat/projectile_system.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"

namespace
{
    simple_platformer::Actor makeActor(glm::vec2 position, simple_platformer::Team team)
    {
        simple_platformer::Actor actor;
        actor.body.bounds = {position, {10.0F, 10.0F}};
        actor.platformerMovement = simple_platformer::PlatformerMovement{};
        actor.health = simple_platformer::Health{3, 3};
        actor.team = team;
        return actor;
    }

    simple_platformer::Projectile makeProjectile()
    {
        simple_platformer::Projectile projectile;
        projectile.bounds = {{0.0F, 4.0F}, {2.0F, 2.0F}};
        projectile.velocity = {100.0F, 0.0F};
        projectile.damage = 1;
        projectile.remainingLifetime = 2.0F;
        projectile.team = simple_platformer::Team::Player;
        projectile.sprite.size = projectile.bounds.size;
        return projectile;
    }

    int healthOf(const simple_platformer::World& world, simple_platformer::ActorId id)
    {
        const simple_platformer::Actor* actor = world.findActor(id);
        if (actor == nullptr || !actor->health.has_value())
        {
            throw std::logic_error("Test actor has no health");
        }
        return actor->health->current;
    }

    const simple_platformer::TileMap EmptyMap =
        simple_platformer::TileMap::fromAscii({"..........", "..........", ".........."});
}

TEST_CASE("A projectile damages the earliest opposing actor and disappears", "[combat][projectile]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId near =
        world.addActor(makeActor({30.0F, 0.0F}, simple_platformer::Team::Enemy));
    const simple_platformer::ActorId far =
        world.addActor(makeActor({45.0F, 0.0F}, simple_platformer::Team::Enemy));
    world.addProjectile(makeProjectile());
    simple_platformer::WorldRequests requests;

    simple_platformer::updateProjectiles(EmptyMap, world, requests, 0.5F);

    REQUIRE(healthOf(world, near) == 3);
    REQUIRE(world.projectiles().size() == 1);
    REQUIRE(world.projectileBursts().empty());
    simple_platformer::updateLifeState(world, requests, 0.0F);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(healthOf(world, near) == 2);
    REQUIRE(healthOf(world, far) == 3);
    REQUIRE(world.projectiles().empty());
    REQUIRE(world.projectileBursts().size() == 1);
    REQUIRE(
        world.projectileBursts().front().cause == simple_platformer::ProjectileBurstCause::Impact);
    REQUIRE(world.projectileBursts().front().center == glm::vec2{29.0F, 5.0F});
    REQUIRE(world.projectileBursts().front().direction == glm::vec2{100.0F, 0.0F});
}

TEST_CASE("A solid tile stops a projectile before an actor", "[combat][projectile]")
{
    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({"...#......", "..........", ".........."});
    simple_platformer::World world;
    const simple_platformer::ActorId target =
        world.addActor(makeActor({70.0F, 0.0F}, simple_platformer::Team::Enemy));
    world.addProjectile(makeProjectile());
    simple_platformer::WorldRequests requests;

    simple_platformer::updateProjectiles(map, world, requests, 1.0F);
    simple_platformer::updateLifeState(world, requests, 0.0F);
    simple_platformer::applyWorldRequests(world, requests);

    REQUIRE(healthOf(world, target) == 3);
    REQUIRE(world.projectiles().empty());
    REQUIRE(world.projectileBursts().size() == 1);
    REQUIRE(
        world.projectileBursts().front().cause == simple_platformer::ProjectileBurstCause::Impact);
    REQUIRE(world.projectileBursts().front().center == glm::vec2{47.0F, 5.0F});
}

TEST_CASE("Projectiles ignore their owner and actors on the same team", "[combat][projectile]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId owner =
        world.addActor(makeActor({5.0F, 0.0F}, simple_platformer::Team::Player));
    const simple_platformer::ActorId teammate =
        world.addActor(makeActor({20.0F, 0.0F}, simple_platformer::Team::Player));
    const simple_platformer::ActorId enemy =
        world.addActor(makeActor({40.0F, 0.0F}, simple_platformer::Team::Enemy));
    simple_platformer::Projectile projectile = makeProjectile();
    projectile.owner = owner;
    world.addProjectile(projectile);
    simple_platformer::WorldRequests requests;

    simple_platformer::updateProjectiles(EmptyMap, world, requests, 0.5F);
    simple_platformer::updateLifeState(world, requests, 0.0F);
    simple_platformer::applyWorldRequests(world, requests);

    REQUIRE(healthOf(world, owner) == 3);
    REQUIRE(healthOf(world, teammate) == 3);
    REQUIRE(healthOf(world, enemy) == 2);
}

TEST_CASE("A projectile is removed when its lifetime expires", "[combat][projectile]")
{
    simple_platformer::World world;
    simple_platformer::Projectile projectile = makeProjectile();
    projectile.remainingLifetime = 0.1F;
    world.addProjectile(projectile);
    simple_platformer::WorldRequests requests;

    simple_platformer::updateProjectiles(EmptyMap, world, requests, 0.1F);
    REQUIRE(world.projectiles().size() == 1);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(world.projectiles().empty());
    REQUIRE(world.projectileBursts().size() == 1);
    REQUIRE(
        world.projectileBursts().front().cause ==
        simple_platformer::ProjectileBurstCause::LifetimeExpired);
    REQUIRE(world.projectileBursts().front().center == glm::vec2{11.0F, 5.0F});
}

TEST_CASE("A projectile burst expires after its short feedback lifetime", "[combat][projectile]")
{
    simple_platformer::World world;
    world.addActor(makeActor({30.0F, 0.0F}, simple_platformer::Team::Enemy));
    world.addProjectile(makeProjectile());
    simple_platformer::WorldRequests requests;

    simple_platformer::updateProjectiles(EmptyMap, world, requests, 0.5F);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(world.projectileBursts().size() == 1);

    simple_platformer::updateProjectileBursts(world, requests, 0.05F);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(world.projectileBursts().size() == 1);

    simple_platformer::updateProjectileBursts(world, requests, 0.05F);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(world.projectileBursts().empty());
}

TEST_CASE("Separate projectile hits have no shared invulnerability", "[combat][projectile]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId target =
        world.addActor(makeActor({30.0F, 0.0F}, simple_platformer::Team::Enemy));
    world.addProjectile(makeProjectile());
    world.addProjectile(makeProjectile());
    simple_platformer::WorldRequests requests;

    simple_platformer::updateProjectiles(EmptyMap, world, requests, 0.5F);
    simple_platformer::updateLifeState(world, requests, 0.0F);
    simple_platformer::applyWorldRequests(world, requests);

    REQUIRE(healthOf(world, target) == 1);
    REQUIRE(world.projectiles().empty());
    REQUIRE(world.projectileBursts().size() == 2);
}

TEST_CASE("Projectile updates reject invalid timing", "[combat][projectile]")
{
    simple_platformer::World world;
    simple_platformer::WorldRequests requests;
    REQUIRE_THROWS_AS(
        simple_platformer::updateProjectiles(EmptyMap, world, requests, -0.1F),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::updateProjectileBursts(world, requests, -0.1F), std::invalid_argument);
}
