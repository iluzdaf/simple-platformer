#pragma once

#include <glm/vec2.hpp>

namespace simple_platformer
{
    struct SpriteRegion
    {
        // Source rectangle measured in texture pixels. The renderer converts it to UVs.
        glm::vec2 position = {0.0F, 0.0F};
        glm::vec2 size = {0.0F, 0.0F};
    };

    struct Sprite
    {
        int textureId = 0;
        SpriteRegion region;
        // Display dimensions measured in world pixels, independent of the collision body.
        glm::vec2 size = {0.0F, 0.0F};
    };

}
