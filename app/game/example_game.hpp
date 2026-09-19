#pragma once

#include <optional>
#include <cstddef>

#include <glm/vec2.hpp>

#include "debug/debug_overlay.hpp"
#include "game/example_content.hpp"
#include "game/level_catalog.hpp"
#include "game/item_catalog.hpp"
#include "simple_platformer/render/camera.hpp"

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
        ExampleGame(int textureId, LevelCatalog catalog);

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
        void loadLevel(int levelNumber);
        void startLevel(Actor player);
        CameraController& cameraControllerValue();
        const CameraController& cameraControllerValue() const;
        Camera currentCamera() const;

        LevelCatalog levelCatalog;
        // Loaded once: inventory IDs keep their meaning across transitions and restarts.
        ItemCatalog itemCatalog;
        GameLevel level;
        std::optional<CameraController> cameraController;
        int atlasTextureId = 0;
        bool gameComplete = false;
    };
}
