#pragma once

#include <vector>

#include <glm/vec2.hpp>

namespace simple_platformer
{
    struct SpriteRegion
    {
        glm::vec2 position = {0.0F, 0.0F};
        glm::vec2 size = {0.0F, 0.0F};
    };

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

    struct Sprite
    {
        int textureId = 0;
        SpriteRegion region;
        glm::vec2 size = {0.0F, 0.0F};
    };

    const SpriteRegion& frameAt(const AnimationClip& clip, float elapsedSeconds);
    AnimationName selectMovementAnimation(bool grounded, glm::vec2 velocity);
}
