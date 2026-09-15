#pragma once

#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    enum class AnimationName
    {
        Idle,
        Run,
        Jump,
        Fall,
        Bite,
        Death
    };

    struct AnimationClip
    {
        AnimationName name = AnimationName::Idle;
        std::vector<SpriteRegion> frames;
        float frameDuration = 0.1F;
        bool looping = true;
    };

    struct Animator
    {
        AnimationName current = AnimationName::Idle;
        float elapsed = 0.0F;
    };

    const SpriteRegion& frameAt(const AnimationClip& clip, float elapsedSeconds);
    void updateAnimation(
        Animator& animator,
        Sprite& sprite,
        AnimationName selected,
        const AnimationClip& clip,
        float deltaTime);
    AnimationName selectMovementAnimation(bool grounded, glm::vec2 velocity);
}
