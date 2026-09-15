#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "simple_platformer/render/sprite.hpp"

namespace
{
    using simple_platformer::AnimationClip;
    using simple_platformer::AnimationName;
    using simple_platformer::SpriteRegion;
}

TEST_CASE("Looping animation clips wrap around", "[render][animation]")
{
    const AnimationClip clip{
        AnimationName::Run,
        {{{1.0F, 0.0F}, {1.0F, 1.0F}}, {{2.0F, 0.0F}, {1.0F, 1.0F}}},
        0.1F,
        true};

    REQUIRE(simple_platformer::frameAt(clip, 0.0F).position.x == 1.0F);
    REQUIRE(simple_platformer::frameAt(clip, 0.1F).position.x == 2.0F);
    REQUIRE(simple_platformer::frameAt(clip, 0.2F).position.x == 1.0F);
}

TEST_CASE("Non-looping animation clips hold their last frame", "[render][animation]")
{
    const AnimationClip clip{
        AnimationName::Death,
        {{{1.0F, 0.0F}, {1.0F, 1.0F}}, {{2.0F, 0.0F}, {1.0F, 1.0F}}},
        0.1F,
        false};

    REQUIRE(simple_platformer::frameAt(clip, 10.0F).position.x == 2.0F);
}

TEST_CASE(
    "Movement animation selection observes grounded state and velocity",
    "[render][animation]")
{
    REQUIRE(simple_platformer::selectMovementAnimation(true, {0.0F, 0.0F}) == AnimationName::Idle);
    REQUIRE(simple_platformer::selectMovementAnimation(true, {1.0F, 0.0F}) == AnimationName::Run);
    REQUIRE(
        simple_platformer::selectMovementAnimation(false, {0.0F, -1.0F}) == AnimationName::Jump);
    REQUIRE(simple_platformer::selectMovementAnimation(false, {0.0F, 1.0F}) == AnimationName::Fall);
}

TEST_CASE("Animation clips reject missing frames and invalid timing", "[render][animation]")
{
    const AnimationClip empty;
    REQUIRE_THROWS_AS(simple_platformer::frameAt(empty, 0.0F), std::invalid_argument);

    const AnimationClip invalidDuration{
        AnimationName::Idle, {SpriteRegion{{0.0F, 0.0F}, {1.0F, 1.0F}}}, 0.0F, true};
    REQUIRE_THROWS_AS(simple_platformer::frameAt(invalidDuration, 0.0F), std::invalid_argument);
}
