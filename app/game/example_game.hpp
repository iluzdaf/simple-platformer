#pragma once

#include <optional>
#include <cstddef>

#include <glm/vec2.hpp>

#include "debug/debug_overlay.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    struct Health;
    struct InputIntentions;
    struct RenderScene;
    class Inventory;
    struct ItemDefinition;

    class ExampleGame
    {
    public:
        explicit ExampleGame(int textureId);

        void update(const InputIntentions& intentions, float deltaTime);
        glm::vec2 playerAimDirection(glm::vec2 screenPosition) const;
        RenderScene buildScene() const;
        DebugOverlay debugOverlay() const;
        Health playerHealth() const;
        // Use these references immediately. Changing or restarting the level replaces the World,
        // so do not store a returned reference for later.
        const Inventory& playerInventory() const;
        const ItemDefinition& itemDefinition(int id) const;
        void useInventoryItem(std::size_t slot);
        void restart();
        int levelNumber() const;
        bool complete() const;
        std::optional<glm::vec2> levelExitScreenPosition() const;
        bool exitReady() const;

    private:
        void loadLevel(int level);
        CameraController& cameraControllerValue();
        const CameraController& cameraControllerValue() const;
        Camera currentCamera() const;

        TileMap map;
        World world;
        std::optional<CameraController> cameraController;
        int atlasTextureId = 0;
        int currentLevel = 0;
        bool gameComplete = false;
    };
}
