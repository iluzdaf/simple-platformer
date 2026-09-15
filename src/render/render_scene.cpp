#include "simple_platformer/render/render_scene.hpp"

#include <algorithm>
#include <cmath>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    RenderScene buildRenderScene(
        const TileMap& map,
        int tileTextureId,
        const Camera& camera,
        const Sprite& playerSprite,
        const Aabb& playerBounds,
        Facing playerFacing)
    {
        RenderScene scene;
        const float tileSize = static_cast<float>(TileSize);
        const int firstColumn =
            std::max(0, static_cast<int>(std::floor(camera.position.x / tileSize)));
        const int lastColumn = std::min(
            map.width() - 1,
            static_cast<int>(std::ceil((camera.position.x + camera.viewportSize.x) / tileSize)) -
                1);
        const int firstRow =
            std::max(0, static_cast<int>(std::floor(camera.position.y / tileSize)));
        const int lastRow = std::min(
            map.height() - 1,
            static_cast<int>(std::ceil((camera.position.y + camera.viewportSize.y) / tileSize)) -
                1);

        for (int row = firstRow; row <= lastRow; ++row)
        {
            for (int column = firstColumn; column <= lastColumn; ++column)
            {
                const GridPosition tilePosition{column, row};
                if (map.tileAt(tilePosition) == 0)
                {
                    continue;
                }

                const glm::vec2 worldPosition = {
                    static_cast<float>(column * TileSize), static_cast<float>(row * TileSize)};
                scene.sprites.push_back(
                    {tileTextureId,
                     worldToScreen(camera, worldPosition),
                     {tileSize, tileSize},
                     map.definitionAt(tilePosition).sprite,
                     false});
            }
        }

        const glm::vec2 playerFeet = feetOf(playerBounds);
        const glm::vec2 playerSpritePosition = {
            playerFeet.x - playerSprite.size.x * 0.5F, playerFeet.y - playerSprite.size.y};
        scene.sprites.push_back(
            {playerSprite.textureId,
             worldToScreen(camera, playerSpritePosition),
             playerSprite.size,
             playerSprite.region,
             playerFacing == Facing::Left});
        return scene;
    }
}
