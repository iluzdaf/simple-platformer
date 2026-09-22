#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"

namespace simple_platformer
{
    class TileMap;

    struct Camera
    {
        // Top-left of the viewport in world pixels.
        glm::vec2 position = {0.0F, 0.0F};
        glm::vec2 viewportSize = InternalViewportSize;
    };

    struct CameraController
    {
        CameraController(Camera initialCamera, glm::vec2 initialDeadZoneSize);

        Camera camera;
        glm::vec2 deadZoneSize;
    };

    Camera makeLockedCamera(
        const TileMap& map,
        const Aabb& target,
        glm::vec2 viewportSize = InternalViewportSize);

    CameraController makeCameraController(
        const TileMap& map,
        const Aabb& target,
        glm::vec2 deadZoneSize,
        glm::vec2 viewportSize = InternalViewportSize);

    void followTarget(CameraController& controller, const TileMap& map, const Aabb& target);

    glm::vec2 worldToScreen(const Camera& camera, glm::vec2 worldPosition);
    glm::vec2 screenToWorld(const Camera& camera, glm::vec2 screenPosition);
}
