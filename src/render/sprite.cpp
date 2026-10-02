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
            const glm::vec2 size = sprite.region.size;
            return {{bodyFeet.x - size.x * 0.5F, bodyFeet.y - size.y}, size};
        }
        case SpriteAnchor::BodyCenter:
            return boxCenteredOn(centerOf(bodyBounds), sprite.region.size);
        }

        throw std::invalid_argument("Sprite anchor is invalid");
    }
}
