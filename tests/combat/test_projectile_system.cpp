#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/actor/lifecycle.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/combat/projectile_system.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/tile_map_builder.hpp"

namespace
{
    simple_platformer::Actor makeActor(glm::vec2 topLeft, simple_platformer::Team team)
    {
        return tests::ActorBuilder::sized({10.0F, 10.0F})
            .at(topLeft)
            .walking()
            .withHealth(3, 3)
            .onTeam(team);
    }

    simple_platformer::Projectile makeProjectile()
    {
        simple_platformer::Projectile projectile;
        projectile.bounds = {{0.0F, 4.0F}, {2.0F, 2.0F}};
        projectile.velocity = {100.0F, 0.0F};
        projectile.damage = 1;
        projectile.lifetimeRemaining = 2.0F;
        projectile.team = simple_platformer::Team::Player;
        projectile.sprite.size = projectile.bounds.size;
        return projectile;
    }

    simple_platformer::TileMap emptyMap()
    {
        return tests::TileMapBuilder({"..........", "..........", ".........."});
    }

    // One tile at column 3 of the top row, on the projectile's path.
    simple_platformer::TileMap mapWithTileInPath(tests::Tile tile)
    {
        return tests::TileMapBuilder({"...X......", "..........", ".........."}).where('X', tile);
    }
}

TEST_CASE("A projectile damages the earliest opposing actor and disappears", "[combat][projectile]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId nearTarget =
        world.addActor(makeActor({30.0F, 0.0F}, simple_platformer::Team::Enemy));
    const simple_platformer::ActorId farTarget =
        world.addActor(makeActor({45.0F, 0.0F}, simple_platformer::Team::Enemy));
    world.addProjectile(makeProjectile());
    simple_platformer::WorldRequests requests;

    simple_platformer::TileMap map = emptyMap();
    simple_platformer::updateProjectiles(map, world, requests, 0.5F);

    REQUIRE(tests::health(world, nearTarget).current == 3);
    REQUIRE(world.projectiles().size() == 1);
    REQUIRE(world.projectileBursts().empty());
    simple_platformer::updateLifeState(world, requests, 0.0F);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::health(world, nearTarget).current == 2);
    REQUIRE(tests::health(world, farTarget).current == 3);
    REQUIRE(world.projectiles().empty());
    REQUIRE(world.projectileBursts().size() == 1);
    REQUIRE(
        world.projectileBursts().front().cause == simple_platformer::ProjectileBurstCause::Impact);
    REQUIRE(world.projectileBursts().front().center == glm::vec2{29.0F, 5.0F});
    REQUIRE(world.projectileBursts().front().direction == glm::vec2{100.0F, 0.0F});
}

TEST_CASE("A solid tile stops a projectile before an actor", "[combat][projectile]")
{
    simple_platformer::TileMap map =
        tests::TileMapBuilder({"...#......", "..........", ".........."});
    simple_platformer::World world;
    const simple_platformer::ActorId target =
        world.addActor(makeActor({70.0F, 0.0F}, simple_platformer::Team::Enemy));
    world.addProjectile(makeProjectile());
    simple_platformer::WorldRequests requests;

    simple_platformer::updateProjectiles(map, world, requests, 1.0F);
    simple_platformer::updateLifeState(world, requests, 0.0F);
    simple_platformer::applyWorldRequests(world, requests);

    REQUIRE(tests::health(world, target).current == 3);
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

    simple_platformer::TileMap map = emptyMap();
    simple_platformer::updateProjectiles(map, world, requests, 0.5F);
    simple_platformer::updateLifeState(world, requests, 0.0F);
    simple_platformer::applyWorldRequests(world, requests);

    REQUIRE(tests::health(world, owner).current == 3);
    REQUIRE(tests::health(world, teammate).current == 3);
    REQUIRE(tests::health(world, enemy).current == 2);
}

TEST_CASE("A projectile is removed when its lifetime expires", "[combat][projectile]")
{
    simple_platformer::World world;
    simple_platformer::Projectile projectile = makeProjectile();
    projectile.lifetimeRemaining = 0.1F;
    world.addProjectile(projectile);
    simple_platformer::WorldRequests requests;

    simple_platformer::TileMap map = emptyMap();
    simple_platformer::updateProjectiles(map, world, requests, 0.1F);
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

    simple_platformer::TileMap map = emptyMap();
    simple_platformer::updateProjectiles(map, world, requests, 0.5F);
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

    simple_platformer::TileMap map = emptyMap();
    simple_platformer::updateProjectiles(map, world, requests, 0.5F);
    simple_platformer::updateLifeState(world, requests, 0.0F);
    simple_platformer::applyWorldRequests(world, requests);

    REQUIRE(tests::health(world, target).current == 1);
    REQUIRE(world.projectiles().empty());
    REQUIRE(world.projectileBursts().size() == 2);
}

TEST_CASE("Projectile updates reject invalid timing", "[combat][projectile]")
{
    simple_platformer::World world;
    simple_platformer::WorldRequests requests;
    simple_platformer::TileMap map = emptyMap();
    REQUIRE_THROWS_AS(
        simple_platformer::updateProjectiles(map, world, requests, -0.1F), std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::updateProjectileBursts(world, requests, -0.1F), std::invalid_argument);
}

TEST_CASE("A projectile that breaks tiles clears the glass it stops at", "[combat][projectile]")
{
    simple_platformer::TileMap map =
        mapWithTileInPath(tests::Tile().blocksMovement().breaksInto('.'));
    simple_platformer::World world;
    simple_platformer::Projectile projectile = makeProjectile();
    projectile.breaksTiles = true;
    world.addProjectile(projectile);
    simple_platformer::WorldRequests requests;

    REQUIRE(map.blocksMovement({3, 0}));
    simple_platformer::updateProjectiles(map, world, requests, 1.0F);
    simple_platformer::applyWorldRequests(world, requests);

    REQUIRE(map.tileAt({3, 0}) == 0);
    REQUIRE_FALSE(map.blocksMovement({3, 0}));
    // The shot is still spent on the tile it broke rather than carrying on through.
    REQUIRE(world.projectiles().empty());
    REQUIRE(world.projectileBursts().size() == 1);
}

TEST_CASE("A projectile without the flag stops at glass and leaves it", "[combat][projectile]")
{
    simple_platformer::TileMap map =
        mapWithTileInPath(tests::Tile().blocksMovement().breaksInto('.'));
    simple_platformer::World world;
    // makeProjectile leaves breaksTiles false, as an enemy weapon does.
    world.addProjectile(makeProjectile());
    simple_platformer::WorldRequests requests;

    simple_platformer::updateProjectiles(map, world, requests, 1.0F);
    simple_platformer::applyWorldRequests(world, requests);

    REQUIRE(map.tileAt({3, 0}) == 1);
    REQUIRE(map.blocksMovement({3, 0}));
    REQUIRE(world.projectiles().empty());
}

TEST_CASE("Breaking projectiles leave unbreakable tiles standing", "[combat][projectile]")
{
    simple_platformer::TileMap map =
        mapWithTileInPath(tests::Tile().blocksMovement().blocksSight());
    simple_platformer::World world;
    simple_platformer::Projectile projectile = makeProjectile();
    projectile.breaksTiles = true;
    world.addProjectile(projectile);
    simple_platformer::WorldRequests requests;

    simple_platformer::updateProjectiles(map, world, requests, 1.0F);
    simple_platformer::applyWorldRequests(world, requests);

    REQUIRE(map.tileAt({3, 0}) == 1);
    REQUIRE(map.blocksMovement({3, 0}));
}

TEST_CASE("One shot cannot open a hole for another in the same frame", "[combat][projectile]")
{
    simple_platformer::TileMap map =
        mapWithTileInPath(tests::Tile().blocksMovement().breaksInto('.'));
    simple_platformer::World world;
    simple_platformer::Projectile shot = makeProjectile();
    shot.breaksTiles = true;
    world.addProjectile(shot);
    world.addProjectile(shot);
    simple_platformer::WorldRequests requests;

    simple_platformer::updateProjectiles(map, world, requests, 1.0F);

    // Both shots were traced against the glass before any of it broke, so neither
    // travelled past it. Glass spans x 48 to 64 on the row the shots follow.
    for (const simple_platformer::Projectile& projectile : world.projectiles())
    {
        REQUIRE(projectile.bounds.position.x < 48.0F);
    }
    REQUIRE(map.tileAt({3, 0}) == 0);

    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(world.projectiles().empty());
    REQUIRE(world.projectileBursts().size() == 2);
}
