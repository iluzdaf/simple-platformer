#include "simple_platformer/render/animation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "simple_platformer/render/sprite.hpp"

namespace
{
    simple_platformer::AnimationName selectMovementAnimation(bool grounded, glm::vec2 velocity)
    {
        using simple_platformer::AnimationName;

        if (!grounded)
        {
            return velocity.y < 0.0F ? AnimationName::Jump : AnimationName::Fall;
        }

        return velocity == glm::vec2{0.0F, 0.0F} ? AnimationName::Idle : AnimationName::Move;
    }
}

namespace simple_platformer
{
    const AnimationClip& clipFor(const AnimationSet& animationSet, AnimationName name)
    {
        const auto clip = std::find_if(
            animationSet.clips.begin(),
            animationSet.clips.end(),
            [name](const AnimationClip& candidate) { return candidate.name == name; });
        if (clip == animationSet.clips.end())
        {
            throw std::invalid_argument(
                "The animation set does not contain the selected animation");
        }
        return *clip;
    }

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
        float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0F)
        {
            throw std::invalid_argument("Animation delta time must be finite and non-negative");
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

        sprite.region = frameAt(clipFor(animator.animationSet, selected), animator.elapsed);
    }

    AnimationName selectActorAnimation(
        bool dying,
        bool attacking,
        bool grounded,
        glm::vec2 velocity)
    {
        if (dying)
        {
            return AnimationName::Death;
        }
        if (attacking)
        {
            return AnimationName::Attack;
        }
        return selectMovementAnimation(grounded, velocity);
    }
}
