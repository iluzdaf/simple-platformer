#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <utility>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/npc_machine_builder.hpp"
#include "support/actor_components.hpp"

namespace
{
    template <typename Builder, typename = void> struct CanWalk : std::false_type
    {
    };

    template <typename Builder>
    struct CanWalk<Builder, std::void_t<decltype(std::declval<Builder>().walking())>>
        : std::true_type
    {
    };
}

static_assert(!CanWalk<tests::ActorBuilder::Sized>::value);
static_assert(CanWalk<tests::ActorBuilder::Placed>::value);
static_assert(!std::is_convertible_v<tests::ActorBuilder::Sized, simple_platformer::Actor>);
static_assert(!std::is_convertible_v<tests::ActorBuilder::Placed, simple_platformer::Actor>);
static_assert(std::is_convertible_v<tests::ActorBuilder, simple_platformer::Actor>);

TEST_CASE(
    "The actor builder places a body by its corner, its feet, or its cell",
    "[support][actor-builder]")
{
    const simple_platformer::Actor byCorner =
        tests::ActorBuilder::sized({12.0F, 20.0F}).at({8.0F, 4.0F}).walking();
    const simple_platformer::Actor byFeet =
        tests::ActorBuilder::sized({12.0F, 20.0F}).atFeet({24.0F, 32.0F}).walking();
    const simple_platformer::Actor byCell =
        tests::ActorBuilder::sized({12.0F, 20.0F}).inCell({1, 1}).walking();

    REQUIRE(byCorner.body.bounds.position == glm::vec2{8.0F, 4.0F});
    REQUIRE(byCorner.body.bounds.size == glm::vec2{12.0F, 20.0F});
    REQUIRE(simple_platformer::feetOf(byFeet.body.bounds) == glm::vec2{24.0F, 32.0F});
    REQUIRE(byFeet.body.bounds.size == glm::vec2{12.0F, 20.0F});
    REQUIRE(simple_platformer::feetOf(byCell.body.bounds) == glm::vec2{24.0F, 32.0F});
    REQUIRE(byCell.body.bounds.size == glm::vec2{12.0F, 20.0F});
}

TEST_CASE("The actor builder gives exactly one movement component", "[support][actor-builder]")
{
    simple_platformer::Actor walker =
        tests::ActorBuilder::sized({12.0F, 20.0F}).at({0.0F, 0.0F}).walking();
    simple_platformer::Actor flyer =
        tests::ActorBuilder::sized({12.0F, 20.0F}).at({0.0F, 0.0F}).flying(40.0F);

    REQUIRE(walker.platformerMovement.has_value());
    REQUIRE_FALSE(walker.flyingMovement.has_value());
    REQUIRE(tests::flyingMovement(flyer).speed == 40.0F);
    REQUIRE_FALSE(flyer.platformerMovement.has_value());
}

TEST_CASE("An NPC from the actor builder is one World accepts", "[support][actor-builder]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId id =
        world.addActor(tests::ActorBuilder::sized({12.0F, 20.0F})
                           .atFeet({24.0F, 32.0F})
                           .walking()
                           .onTeam(simple_platformer::Team::Enemy)
                           .thinking({64.0F, 2.0F})
                           .patrolling({8.0F, 32.0F}, {56.0F, 32.0F})
                           .biting());

    simple_platformer::Actor& npc = tests::actor(world, id);
    REQUIRE(npc.team == simple_platformer::Team::Enemy);
    REQUIRE(tests::brain(npc).state == simple_platformer::NpcState::Idle);
    REQUIRE(tests::senses(npc).noticeDistance == 64.0F);
    REQUIRE(tests::senses(npc).targetMemoryDuration == 2.0F);
    REQUIRE_FALSE(tests::pathFollower(npc).path.has_value());
    REQUIRE(tests::patrol(npc).firstFeet == glm::vec2{8.0F, 32.0F});
    REQUIRE(tests::patrol(npc).secondFeet == glm::vec2{56.0F, 32.0F});
    REQUIRE(npc.bite.has_value());
}

TEST_CASE("A thinking actor from the builder can run a machine", "[support][actor-builder]")
{
    simple_platformer::World world;
    const simple_platformer::ActorId id =
        world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                           .at({8.0F, 8.0F})
                           .walking()
                           .thinking({})
                           .running(tests::NpcMachineBuilder::named("test").state(
                               "rest", simple_platformer::NpcState::Idle)));
    REQUIRE(
        simple_platformer::activeNpcMachineState(
            tests::actor(world, id).machine.value_or(simple_platformer::NpcMachine{}))
            .name == "rest");
}
