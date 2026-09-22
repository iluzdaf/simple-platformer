#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "support/require_near.hpp"

namespace
{
    using simple_platformer::AnimationClip;
    using simple_platformer::AnimationName;
    using simple_platformer::AnimationSet;
    using simple_platformer::Animator;
    using simple_platformer::Sprite;
    using simple_platformer::SpriteRegion;
}

TEST_CASE("Looping animation clips wrap around", "[render][animation]")
{
    const AnimationClip clip{
        AnimationName::Move,
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

TEST_CASE("Each animator keeps independent playback state", "[render][animation]")
{
    const AnimationClip move{
        AnimationName::Move,
        {{{1.0F, 0.0F}, {1.0F, 1.0F}}, {{2.0F, 0.0F}, {1.0F, 1.0F}}},
        0.1F,
        true};
    Animator first;
    Animator second;
    first.animationSet = AnimationSet{{move}};
    second.animationSet = AnimationSet{{move}};
    Sprite firstSprite;
    Sprite secondSprite;

    simple_platformer::updateAnimation(first, firstSprite, AnimationName::Move, 0.1F);
    simple_platformer::updateAnimation(first, firstSprite, AnimationName::Move, 0.1F);
    simple_platformer::updateAnimation(second, secondSprite, AnimationName::Move, 0.1F);

    REQUIRE_NEAR(first.elapsed, 0.1F);
    REQUIRE(firstSprite.region.position.x == 2.0F);
    REQUIRE_NEAR(second.elapsed, 0.0F);
    REQUIRE(secondSprite.region.position.x == 1.0F);
}

TEST_CASE("Changing animation resets its playback time", "[render][animation]")
{
    const AnimationClip death{AnimationName::Death, {{{5.0F, 0.0F}, {1.0F, 1.0F}}}, 0.4F, false};
    Animator animator;
    animator.current = AnimationName::Move;
    animator.elapsed = 0.3F;
    animator.animationSet = AnimationSet{{death}};
    Sprite sprite;

    simple_platformer::updateAnimation(animator, sprite, AnimationName::Death, 0.1F);

    REQUIRE(animator.current == AnimationName::Death);
    REQUIRE_NEAR(animator.elapsed, 0.0F);
    REQUIRE(sprite.region.position == glm::vec2{5.0F, 0.0F});
}

TEST_CASE("Animation sets find clips by name", "[render][animation]")
{
    const AnimationClip idle{
        AnimationName::Idle, {SpriteRegion{{1.0F, 2.0F}, {3.0F, 4.0F}}}, 0.1F, true};
    const AnimationSet animations{{idle}};

    REQUIRE(
        simple_platformer::clipFor(animations, AnimationName::Idle).frames.front().position.x ==
        1.0F);
    REQUIRE_THROWS_AS(
        simple_platformer::clipFor(animations, AnimationName::Death), std::invalid_argument);
}

TEST_CASE(
    "Movement animation selection observes grounded state and velocity",
    "[render][animation]")
{
    REQUIRE(
        simple_platformer::selectActorAnimation(false, false, true, {0.0F, 0.0F}) ==
        AnimationName::Idle);
    REQUIRE(
        simple_platformer::selectActorAnimation(false, false, true, {1.0F, 0.0F}) ==
        AnimationName::Move);
    REQUIRE(
        simple_platformer::selectActorAnimation(false, false, true, {0.0F, 1.0F}) ==
        AnimationName::Move);
    REQUIRE(
        simple_platformer::selectActorAnimation(false, false, false, {0.0F, -1.0F}) ==
        AnimationName::Jump);
    REQUIRE(
        simple_platformer::selectActorAnimation(false, false, false, {0.0F, 1.0F}) ==
        AnimationName::Fall);
}

TEST_CASE("Actor animation selection gives death and attack priority", "[render][animation]")
{
    REQUIRE(
        simple_platformer::selectActorAnimation(true, true, true, {1.0F, 0.0F}) ==
        AnimationName::Death);
    REQUIRE(
        simple_platformer::selectActorAnimation(false, true, true, {1.0F, 0.0F}) ==
        AnimationName::Attack);
    REQUIRE(
        simple_platformer::selectActorAnimation(false, false, true, {1.0F, 0.0F}) ==
        AnimationName::Move);
}

TEST_CASE("Animation clips reject missing frames and invalid timing", "[render][animation]")
{
    const AnimationClip empty;
    REQUIRE_THROWS_AS(simple_platformer::frameAt(empty, 0.0F), std::invalid_argument);

    const AnimationClip invalidDuration{
        AnimationName::Idle, {SpriteRegion{{0.0F, 0.0F}, {1.0F, 1.0F}}}, 0.0F, true};
    REQUIRE_THROWS_AS(simple_platformer::frameAt(invalidDuration, 0.0F), std::invalid_argument);

    Animator animator;
    Sprite sprite;
    const AnimationClip idle{
        AnimationName::Idle, {SpriteRegion{{0.0F, 0.0F}, {1.0F, 1.0F}}}, 0.1F, true};
    animator.animationSet = AnimationSet{{idle}};
    REQUIRE_THROWS_AS(
        simple_platformer::updateAnimation(animator, sprite, AnimationName::Move, 0.1F),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        simple_platformer::updateAnimation(animator, sprite, AnimationName::Idle, -0.1F),
        std::invalid_argument);
}
