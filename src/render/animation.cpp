#include "simple_platformer/render/animation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    const SpriteRegion& frameAt(const AnimationClip& clip, float elapsedSeconds)
    {
        if (clip.frames.empty())
        {
            throw std::invalid_argument("Animation clips require at least one frame");
        }
        if (!std::isfinite(clip.frameDuration) || clip.frameDuration <= 0.0F ||
            !std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0F)
        {
            throw std::invalid_argument("Animation timing must be positive and finite");
        }

        std::size_t frame = static_cast<std::size_t>(elapsedSeconds / clip.frameDuration);
        if (clip.looping)
        {
            frame %= clip.frames.size();
        }
        else
        {
            frame = std::min(frame, clip.frames.size() - 1);
        }

        return clip.frames[frame];
    }

    void updateAnimation(
        Animator& animator,
        Sprite& sprite,
        AnimationName selected,
        const AnimationClip& clip,
        float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0F)
        {
            throw std::invalid_argument("Animation delta time must be finite and non-negative");
        }
        if (clip.name != selected)
        {
            throw std::invalid_argument("The animation clip does not match the selected animation");
        }

        if (animator.current != selected)
        {
            animator.current = selected;
            animator.elapsed = 0.0F;
        }
        else
        {
            animator.elapsed += deltaTime;
        }

        sprite.region = frameAt(clip, animator.elapsed);
    }

    AnimationName selectMovementAnimation(bool grounded, glm::vec2 velocity)
    {
        if (!grounded)
        {
            return velocity.y < 0.0F ? AnimationName::Jump : AnimationName::Fall;
        }

        return velocity.x == 0.0F ? AnimationName::Idle : AnimationName::Run;
    }
}
