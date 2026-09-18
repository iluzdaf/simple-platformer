#pragma once

#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    class TileMap;
    class World;

    struct SpriteDrawCommand
    {
        int textureId = 0;
        glm::vec2 position = {0.0F, 0.0F};
        glm::vec2 size = {0.0F, 0.0F};
        SpriteRegion source;
        bool flipHorizontal = false;
        // Clockwise rotation around the sprite centre.
        float rotationRadians = 0.0F;
        // Multiplies the texture alpha: 1 is opaque and 0 is invisible.
        float opacity = 1.0F;
        // Mixes the sprite colour towards white: 0 is unchanged and 1 is fully white.
        float whiteFlashAmount = 0.0F;
    };

    struct RenderScene
    {
        std::vector<SpriteDrawCommand> sprites;
    };

    RenderScene buildRenderScene(
        const TileMap& map,
        int tileTextureId,
        const Camera& camera,
        const World& world);
}
