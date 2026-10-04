#include <catch2/catch_test_macros.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_behaviour.hpp"

TEST_CASE(
    "Entering an NPC state replaces the state and resets its elapsed time",
    "[npc][behaviour]")
{
    simple_platformer::Actor actor;
    simple_platformer::NpcBrain brain;
    simple_platformer::PathFollower follower;
    brain.state = simple_platformer::NpcState::Chase;
    brain.stateElapsed = 2.0F;

    simple_platformer::enterNpcState(actor, brain, follower, simple_platformer::NpcState::Search);

    REQUIRE(brain.state == simple_platformer::NpcState::Search);
    REQUIRE(brain.stateElapsed == 0.0F);
}

TEST_CASE("Entering an NPC state discards its old path and progress", "[npc][behaviour]")
{
    simple_platformer::Actor actor;
    simple_platformer::NpcBrain brain;
    simple_platformer::PathFollower follower;
    follower.path = simple_platformer::NavigationPath{};
    follower.goal = {64.0F, 32.0F};
    follower.nextStep = 2;
    follower.phase = simple_platformer::PathStepPhase::ReplayInputs;
    follower.programElapsed = 0.3F;

    simple_platformer::enterNpcState(actor, brain, follower, simple_platformer::NpcState::Idle);

    REQUIRE_FALSE(follower.path.has_value());
    REQUIRE_FALSE(follower.goal.has_value());
    REQUIRE(follower.nextStep == 0);
    REQUIRE(follower.phase == simple_platformer::PathStepPhase::ApproachStart);
    REQUIRE(follower.programElapsed == 0.0F);
}
