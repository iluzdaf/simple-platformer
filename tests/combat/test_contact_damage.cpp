#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/lifecycle.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"

namespace
{
    simple_platformer::Actor makeActor(glm::vec2 topLeft, simple_platformer::Team team)
    {
        return tests::ActorBuilder::sized({12.0F, 12.0F})
            .at(topLeft)
            .walking()
            .withHealth(3, 3)
            .onTeam(team);
    }
}

TEST_CASE("Contact damage hits an opponent once per activation, not allies", "[combat][contact]")
{
    simple_platformer::World world;
    simple_platformer::Actor charger = makeActor({20.0F, 20.0F}, simple_platformer::Team::Enemy);
    charger.contactDamage = simple_platformer::ContactDamage{};
    charger.intentions.contactDamage = true;
    const auto chargerId = world.addActor(charger);
    const auto targetId =
        world.addActor(makeActor({26.0F, 20.0F}, simple_platformer::Team::Player));
    const auto allyId = world.addActor(makeActor({26.0F, 20.0F}, simple_platformer::Team::Enemy));
    simple_platformer::WorldRequests requests;

    simple_platformer::updateAttacks(world, requests, 0.1F);
    simple_platformer::updateLifeState(world, requests, 0.0F);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::health(world, targetId).current == 2);
    REQUIRE(tests::health(world, allyId).current == 3);
    simple_platformer::updateAttacks(world, requests, 0.1F);
    simple_platformer::updateLifeState(world, requests, 0.0F);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::health(world, targetId).current == 2);
    REQUIRE(tests::contactDamage(world, chargerId).actorsHit.size() == 1);
    REQUIRE(tests::actor(world, chargerId).body.velocity == glm::vec2{0.0F, 0.0F});

    tests::actor(world, chargerId).intentions.contactDamage = false;
    simple_platformer::updateAttacks(world, requests, 0.1F);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE_FALSE(tests::contactDamage(world, chargerId).active);
    REQUIRE(tests::contactDamage(world, chargerId).actorsHit.empty());
    REQUIRE(tests::health(world, targetId).current == 2);

    tests::actor(world, chargerId).intentions.contactDamage = true;
    simple_platformer::updateAttacks(world, requests, 0.1F);
    simple_platformer::updateLifeState(world, requests, 0.0F);
    simple_platformer::applyWorldRequests(world, requests);
    REQUIRE(tests::health(world, targetId).current == 1);
}

TEST_CASE("Contact damage needs an intention", "[combat][contact]")
{
    simple_platformer::World world;
    simple_platformer::Actor attacker = makeActor({20.0F, 20.0F}, simple_platformer::Team::Enemy);
    attacker.contactDamage = simple_platformer::ContactDamage{};
    const auto attackerId = world.addActor(attacker);
    const auto targetId =
        world.addActor(makeActor({26.0F, 20.0F}, simple_platformer::Team::Player));
    simple_platformer::WorldRequests requests;

    simple_platformer::updateAttacks(world, requests, 0.1F);
    simple_platformer::applyWorldRequests(world, requests);

    REQUIRE(tests::health(world, targetId).current == 3);
    REQUIRE_FALSE(tests::contactDamage(world, attackerId).active);
}

TEST_CASE("A dying owner cannot keep contact damage active", "[combat][contact][lifecycle]")
{
    simple_platformer::World world;
    simple_platformer::Actor attacker = makeActor({20.0F, 20.0F}, simple_platformer::Team::Enemy);
    attacker.contactDamage = simple_platformer::ContactDamage{};
    attacker.intentions.contactDamage = true;
    attacker.contactDamage->active = true;
    attacker.life = simple_platformer::LifeState::Dying;
    const auto attackerId = world.addActor(attacker);
    const auto targetId =
        world.addActor(makeActor({26.0F, 20.0F}, simple_platformer::Team::Player));
    simple_platformer::WorldRequests requests;

    simple_platformer::updateAttacks(world, requests, 0.1F);
    simple_platformer::applyWorldRequests(world, requests);

    REQUIRE(tests::health(world, targetId).current == 3);
    REQUIRE_FALSE(tests::contactDamage(world, attackerId).active);
}
