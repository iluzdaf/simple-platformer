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
