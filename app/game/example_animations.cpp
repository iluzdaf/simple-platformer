#include "example_animations.hpp"

#include <glm/vec2.hpp>

#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace
{
    constexpr glm::vec2 FrameSize = {32.0F, 24.0F};
    constexpr float FastWalkFrameDuration = 0.16F;
    constexpr float ZombieWalkFrameDuration = 0.20F;

    simple_platformer::SpriteRegion frame(float left, float top)
    {
        return {{left, top}, FrameSize};
    }
}

namespace simple_platformer
{
    AnimationSet makePlayerAnimations()
    {
        return {{
            AnimationClip{
                AnimationName::Idle, {frame(0.0F, 0.0F), frame(32.0F, 0.0F)}, 0.30F, true},
            AnimationClip{
                AnimationName::Move,
                {frame(64.0F, 0.0F), frame(128.0F, 0.0F), frame(96.0F, 0.0F), frame(128.0F, 0.0F)},
                FastWalkFrameDuration,
                true},
            AnimationClip{AnimationName::Jump, {frame(0.0F, 24.0F)}, 0.15F, true},
            AnimationClip{AnimationName::Fall, {frame(32.0F, 24.0F)}, 0.15F, true},
            AnimationClip{AnimationName::Attack, {frame(64.0F, 24.0F)}, 0.10F, true},
            AnimationClip{AnimationName::Death, {frame(96.0F, 24.0F)}, 0.40F, false},
        }};
    }

    AnimationSet makeZombieAnimations()
    {
        return {{
            AnimationClip{
                AnimationName::Idle, {frame(0.0F, 48.0F), frame(32.0F, 48.0F)}, 0.30F, true},
            AnimationClip{
                AnimationName::Move,
                {frame(64.0F, 48.0F),
                 frame(128.0F, 48.0F),
                 frame(96.0F, 48.0F),
                 frame(128.0F, 48.0F)},
                ZombieWalkFrameDuration,
                true},
            AnimationClip{AnimationName::Jump, {frame(0.0F, 72.0F)}, 0.15F, true},
            AnimationClip{AnimationName::Fall, {frame(32.0F, 72.0F)}, 0.15F, true},
            AnimationClip{AnimationName::Attack, {frame(64.0F, 72.0F)}, 0.10F, true},
            AnimationClip{AnimationName::Death, {frame(96.0F, 72.0F)}, 0.40F, false},
        }};
    }

    AnimationSet makeBatAnimations()
    {
        return {{
            AnimationClip{
                AnimationName::Idle, {frame(0.0F, 96.0F), frame(32.0F, 96.0F)}, 0.30F, true},
            AnimationClip{
                AnimationName::Move,
                // Wings up, out, down, then folded recovery into the next flap.
                {frame(64.0F, 96.0F),
                 frame(128.0F, 96.0F),
                 frame(96.0F, 96.0F),
                 frame(128.0F, 120.0F)},
                0.10F,
                true},
            AnimationClip{AnimationName::Jump, {frame(0.0F, 120.0F)}, 0.15F, true},
            AnimationClip{AnimationName::Fall, {frame(32.0F, 120.0F)}, 0.15F, true},
            AnimationClip{AnimationName::Attack, {frame(64.0F, 120.0F)}, 0.10F, true},
            AnimationClip{AnimationName::Death, {frame(96.0F, 120.0F)}, 0.40F, false},
        }};
    }

    AnimationSet makeZombieSoldierAnimations()
    {
        return {{
            AnimationClip{
                AnimationName::Idle, {frame(0.0F, 144.0F), frame(32.0F, 144.0F)}, 0.30F, true},
            AnimationClip{
                AnimationName::Move,
                {frame(64.0F, 144.0F),
                 frame(128.0F, 144.0F),
                 frame(96.0F, 144.0F),
                 frame(128.0F, 144.0F)},
                FastWalkFrameDuration,
                true},
            AnimationClip{AnimationName::Jump, {frame(0.0F, 168.0F)}, 0.15F, true},
            AnimationClip{AnimationName::Fall, {frame(32.0F, 168.0F)}, 0.15F, true},
            AnimationClip{AnimationName::Attack, {frame(64.0F, 168.0F)}, 0.10F, true},
            AnimationClip{AnimationName::Death, {frame(96.0F, 168.0F)}, 0.40F, false},
        }};
    }
}
