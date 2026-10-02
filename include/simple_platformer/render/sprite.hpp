#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"

namespace simple_platformer
{
    struct SpriteRegion
    {
        // Source rectangle measured in texture pixels. The renderer converts it to UVs.
        glm::vec2 position = {0.0F, 0.0F};
        glm::vec2 size = {0.0F, 0.0F};
    };

    enum class SpriteAnchor
    {
        BodyFeet,
        BodyCenter
    };

    struct Sprite
    {
        int textureId = 0;
        // Drawn at its size in the atlas: one texture pixel is one world pixel.
        SpriteRegion region;
        // BodyFeet aligns the sprite's bottom centre with the body's feet. BodyCenter
        // aligns their centres. Neither changes the body's size.
        SpriteAnchor anchor = SpriteAnchor::BodyFeet;
    };

    Aabb spriteBounds(const Aabb& bodyBounds, const Sprite& sprite);
}
