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
    void validateViewport(glm::vec2 viewportSize)
    {
        if (!simple_platformer::isFinite(viewportSize) || viewportSize.x <= 0.0F ||
            viewportSize.y <= 0.0F)
        {
            throw std::invalid_argument("Camera viewport must be positive and finite");
        }
    }

    void validateDeadZone(glm::vec2 deadZoneSize, glm::vec2 viewportSize)
    {
        if (!simple_platformer::isFinite(deadZoneSize) || deadZoneSize.x <= 0.0F ||
            deadZoneSize.y <= 0.0F || deadZoneSize.x > viewportSize.x ||
            deadZoneSize.y > viewportSize.y)
        {
            throw std::invalid_argument(
                "Camera dead zone must be positive, finite, and no larger than the viewport");
        }
    }

    void validateCamera(const simple_platformer::Camera& camera)
    {
        validateViewport(camera.viewportSize);
        if (!simple_platformer::isFinite(camera.position))
        {
            throw std::invalid_argument("Camera position must be finite");
        }
    }

    float cameraAxis(float targetCenter, float mapSize, float viewportSize)
    {
        if (mapSize <= viewportSize)
        {
            return std::round((mapSize - viewportSize) * 0.5F);
        }

        return std::round(
            std::clamp(targetCenter - viewportSize * 0.5F, 0.0F, mapSize - viewportSize));
    }

    float followAxis(
        float cameraPosition,
        float targetCenter,
        float mapSize,
        float viewportSize,
        float deadZoneSize)
    {
        if (mapSize <= viewportSize)
        {
            return std::round((mapSize - viewportSize) * 0.5F);
        }

        const float deadZoneStart = cameraPosition + (viewportSize - deadZoneSize) * 0.5F;
        const float deadZoneEnd = deadZoneStart + deadZoneSize;
        if (targetCenter < deadZoneStart)
        {
            cameraPosition -= deadZoneStart - targetCenter;
        }
        else if (targetCenter > deadZoneEnd)
        {
            cameraPosition += targetCenter - deadZoneEnd;
        }

        return std::round(std::clamp(cameraPosition, 0.0F, mapSize - viewportSize));
    }
}

namespace simple_platformer
{
    CameraController::CameraController(Camera initialCamera, glm::vec2 initialDeadZoneSize)
        : camera(initialCamera),
          deadZoneSize(initialDeadZoneSize)
    {
        validateCamera(camera);
        validateDeadZone(deadZoneSize, camera.viewportSize);
    }

    Camera makeLockedCamera(const TileMap& map, const Aabb& target, glm::vec2 viewportSize)
    {
        validateViewport(viewportSize);

        const glm::vec2 targetCenter = centerOf(target);
        Camera camera;
        camera.viewportSize = viewportSize;
        camera.position = {
            cameraAxis(targetCenter.x, map.pixelWidth(), viewportSize.x),
            cameraAxis(targetCenter.y, map.pixelHeight(), viewportSize.y)};
        return camera;
    }

    CameraController makeCameraController(
        const TileMap& map,
        const Aabb& target,
        glm::vec2 deadZoneSize,
        glm::vec2 viewportSize)
    {
        return CameraController{makeLockedCamera(map, target, viewportSize), deadZoneSize};
    }

    void followTarget(CameraController& controller, const TileMap& map, const Aabb& target)
    {
        validateCamera(controller.camera);
        validateDeadZone(controller.deadZoneSize, controller.camera.viewportSize);

        const glm::vec2 targetCenter = centerOf(target);
        controller.camera.position = {
            followAxis(
                controller.camera.position.x,
                targetCenter.x,
                map.pixelWidth(),
                controller.camera.viewportSize.x,
                controller.deadZoneSize.x),
            followAxis(
                controller.camera.position.y,
                targetCenter.y,
                map.pixelHeight(),
                controller.camera.viewportSize.y,
                controller.deadZoneSize.y)};
    }

    glm::vec2 worldToScreen(const Camera& camera, glm::vec2 worldPosition)
    {
        return worldPosition - camera.position;
    }

    glm::vec2 screenToWorld(const Camera& camera, glm::vec2 screenPosition)
    {
        return screenPosition + camera.position;
    }
}
