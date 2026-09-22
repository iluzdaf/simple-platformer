#pragma once

#include "simple_platformer/render/animation.hpp"

namespace tests
{
    // A one-frame looping clip. Its atlas column is what tells one clip from another.
    inline simple_platformer::AnimationClip clip(simple_platformer::AnimationName name, float left)
    {
        return {name, {{{left, 0.0F}, {1.0F, 1.0F}}}, 0.1F, true};
    }

    // An animator with a clip for every AnimationName, so whichever the system selects is
    // there to be read back.
    inline simple_platformer::Animator fullAnimator()
    {
        using simple_platformer::AnimationName;

        simple_platformer::Animator animator;
        animator.animationSet = {{
            clip(AnimationName::Idle, 0.0F),
            clip(AnimationName::Move, 1.0F),
            clip(AnimationName::Jump, 2.0F),
            clip(AnimationName::Fall, 3.0F),
            clip(AnimationName::Attack, 4.0F),
            clip(AnimationName::Death, 5.0F),
        }};
        return animator;
    }
}
