#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "debug_overlay.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    struct InputIntentions;
    struct RenderScene;

    class ExampleGame
    {
    public:
        explicit ExampleGame(int textureId);

        void update(const InputIntentions& intentions, float deltaTime);
        glm::vec2 playerAimDirection(glm::vec2 screenPosition) const;
        RenderScene buildScene() const;
        DebugOverlay debugOverlay() const;

    private:
        CameraController& cameraControllerValue();
        const CameraController& cameraControllerValue() const;
        Camera currentCamera() const;

        TileMap map;
        World world;
        std::optional<CameraController> cameraController;
    };
}
