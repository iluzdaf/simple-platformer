#include "simple_platformer/render/camera.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace
{
    float cameraAxis(float targetCenter, float mapSize, float viewportSize)
    {
        if (mapSize <= viewportSize)
        {
            return std::round((mapSize - viewportSize) * 0.5F);
        }

        return std::round(
            std::clamp(targetCenter - viewportSize * 0.5F, 0.0F, mapSize - viewportSize));
    }
}

namespace simple_platformer
{
    Camera makeLockedCamera(const TileMap& map, const Aabb& target, glm::vec2 viewportSize)
    {
        if (!isFinite(viewportSize) || viewportSize.x <= 0.0F || viewportSize.y <= 0.0F)
        {
            throw std::invalid_argument("Camera viewport must be positive and finite");
        }

        const glm::vec2 targetCenter = centerOf(target);
        Camera camera;
        camera.viewportSize = viewportSize;
        camera.position = {
            cameraAxis(targetCenter.x, map.pixelWidth(), viewportSize.x),
            cameraAxis(targetCenter.y, map.pixelHeight(), viewportSize.y)};
        return camera;
    }

    glm::vec2 worldToScreen(const Camera& camera, glm::vec2 worldPosition)
    {
        return worldPosition - camera.position;
    }
}
