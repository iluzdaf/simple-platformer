#pragma once

#include <optional>
#include <cstddef>

#include <glm/vec2.hpp>

#include "debug/debug_overlay.hpp"
#include "game/level_composition.hpp"
#include "content/level_catalog.hpp"
#include "content/game_catalogs.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "lua_npc_scripts.hpp"

namespace simple_platformer
{
    // How long the screen keeps hinting after the player last stood in a locked exit.
    constexpr float LockedExitHintSeconds = 1.0F;

    struct FrameProfile;
    struct Health;
    struct InputIntentions;
    struct RenderScene;
    class Inventory;
    struct ItemDefinition;

    class Game
    {
    public:
        // Catalogs and scripts arrive loaded. Each level queues navigation work for
        // the caller's fixed simulation step; later updates fill the cache.
        Game(
            int textureId,
            LevelCatalog levelCatalog,
            GameCatalogs gameCatalogs,
            LuaNpcScripts npcScripts,
            float simulationStepSeconds);

        // With a profile, the simulation charges each of its phases to it.
        void update(
            const InputIntentions& intentions,
            float deltaTime,
            FrameProfile* profile = nullptr);
        glm::vec2 playerAimDirection(glm::vec2 screenPosition) const;
        RenderScene buildScene() const;
        // The atlas width comes from whoever loaded the texture; the game knows only its id.
        // The cursor, in internal pixels, picks the cell whose navigation is shown, and the
        // profile index selects which NPC navigation profile to show.
        DebugOverlay debugOverlay(
            float atlasWidth,
            std::optional<glm::vec2> internalCursor,
            std::size_t navigationProfileIndex,
            std::optional<ActorId> lockedMachineActor = std::nullopt) const;
        std::optional<ActorId> machineActorAt(glm::vec2 internalPosition) const;
        Health playerHealth() const;
        // Use these references immediately. Changing or restarting the level replaces the World,
        // so do not store a returned reference for later.
        const Inventory& playerInventory() const;
        const ItemDefinition& itemDefinition(int id) const;
        void useInventoryItem(std::size_t slot);
        // Breaks the tile under this internal position as a projectile would, for trying a
        // break without one. Reports whether a tile broke.
        bool breakTileAt(glm::vec2 internalPosition);
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
        // Reuse the same definitions across transitions and restarts.
        GameCatalogs gameCatalogs;
        LuaNpcScripts npcScripts;
        GameLevel level;
        std::optional<CameraController> cameraController;
        int atlasTextureId = 0;
        float simulationStepSeconds = 0.0F;
        bool gameComplete = false;
    };
}
