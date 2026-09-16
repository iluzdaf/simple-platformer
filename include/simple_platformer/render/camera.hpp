#pragma once

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"

namespace simple_platformer
{
    class TileMap;

    struct Camera
    {
        glm::vec2 position = {0.0F, 0.0F};
        glm::vec2 viewportSize = {320.0F, 180.0F};
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
        glm::vec2 viewportSize = {320.0F, 180.0F});

    CameraController makeCameraController(
        const TileMap& map,
        const Aabb& target,
        glm::vec2 deadZoneSize,
        glm::vec2 viewportSize = {320.0F, 180.0F});

    void followTarget(CameraController& controller, const TileMap& map, const Aabb& target);

    glm::vec2 worldToScreen(const Camera& camera, glm::vec2 worldPosition);
    glm::vec2 screenToWorld(const Camera& camera, glm::vec2 screenPosition);
}
