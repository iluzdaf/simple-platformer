#pragma once

#include <optional>
#include <cstddef>

#include <glm/vec2.hpp>

#include "debug/debug_overlay.hpp"
#include "game/level_composition.hpp"
#include "content/level_catalog.hpp"
#include "content/game_catalogs.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    // How long the screen keeps hinting after the player last stood in a locked exit.
    constexpr float LockedExitHintSeconds = 1.0F;

    struct Health;
    struct InputIntentions;
    struct RenderScene;
    class Inventory;
    struct ItemDefinition;

    class Game
    {
    public:
        explicit Game(int textureId);
        Game(int textureId, LevelCatalog catalog);

        void update(const InputIntentions& intentions, float deltaTime);
        glm::vec2 playerAimDirection(glm::vec2 screenPosition) const;
        RenderScene buildScene() const;
        // The atlas width comes from whoever loaded the texture; the game knows only its id.
        DebugOverlay debugOverlay(float atlasWidth) const;
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
        // The icon of what the exit needs, while the player has just tried it without.
        std::optional<Sprite> lockedExitHintIcon() const;

    private:
        void loadLevel(int levelNumber);
        void startLevel(Actor player);
        CameraController& cameraControllerValue();
        const CameraController& cameraControllerValue() const;
        Camera currentCamera() const;

        LevelCatalog levelCatalog;
        // Loaded once: definitions stay consistent across transitions and restarts.
        GameCatalogs catalogs;
        GameLevel level;
        std::optional<CameraController> cameraController;
        int atlasTextureId = 0;
        bool gameComplete = false;
    };
}
