#include "simple_platformer/render/sprite.hpp"

#include <stdexcept>

#include "simple_platformer/math/aabb.hpp"

namespace simple_platformer
{
    Aabb spriteBounds(const Aabb& bodyBounds, const Sprite& sprite)
    {
        switch (sprite.anchor)
        {
        case SpriteAnchor::BodyFeet: {
            const glm::vec2 bodyFeet = feetOf(bodyBounds);
            return {{bodyFeet.x - sprite.size.x * 0.5F, bodyFeet.y - sprite.size.y}, sprite.size};
        }
        case SpriteAnchor::BodyCenter:
            return {centerOf(bodyBounds) - sprite.size * 0.5F, sprite.size};
        }

        throw std::invalid_argument("Sprite anchor is invalid");
    }
}
