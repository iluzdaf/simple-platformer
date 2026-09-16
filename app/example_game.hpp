#pragma once

#include <glm/vec2.hpp>

#include "actor_debug.hpp"
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
        ActorDebugScene actorDebugScene() const;

    private:
        Camera currentCamera() const;

        TileMap map;
        World world;
        CameraController cameraController;
    };
}
