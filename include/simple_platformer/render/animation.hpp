#pragma once

#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    enum class AnimationName
    {
        Idle,
        Move,
        Jump,
        Fall,
        Attack,
        Death
    };

    struct AnimationClip
    {
        AnimationName name = AnimationName::Idle;
        std::vector<SpriteRegion> frames;
        float frameDuration = 0.1F;
        bool looping = true;
    };

    struct AnimationSet
    {
        std::vector<AnimationClip> clips;
    };

    struct Animator
    {
        AnimationName current = AnimationName::Idle;
        float elapsed = 0.0F;
        AnimationSet animationSet;
    };

    const AnimationClip& clipFor(const AnimationSet& animationSet, AnimationName name);
    const SpriteRegion& frameAt(const AnimationClip& clip, float elapsedSeconds);
    void updateAnimation(
        Animator& animator,
        Sprite& sprite,
        AnimationName selected,
        float deltaTime);
    AnimationName selectActorAnimation(
        bool dying,
        bool attacking,
        bool grounded,
        glm::vec2 velocity);
}
