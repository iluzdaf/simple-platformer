#include "example_game.hpp"

#include "debug/debug_overlay.hpp"
#include "example_content.hpp"
#include "level_catalog.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/render/animation_system.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/render_scene.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/level_validation.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "simple_platformer/world/world_simulation.hpp"

namespace simple_platformer
{
    ExampleGame::ExampleGame(int textureId) : ExampleGame(textureId, loadLevelCatalog())
    {
    }

    ExampleGame::ExampleGame(int textureId, LevelCatalog catalog)
        : levelCatalog(std::move(catalog)),
          level(makeGameLevel(levelCatalog, levelCatalog.startLevel, textureId)),
          atlasTextureId(textureId)
    {
        startLevel(makePlayer(levelCatalog, atlasTextureId));
    }

    void ExampleGame::loadLevel(int levelNumber)
    {
        Actor nextPlayer = makePlayer(levelCatalog, atlasTextureId);
        if (const Actor* previousPlayer = level.world.findActor(level.world.playerId()))
        {
            nextPlayer.health = previousPlayer->health;
            nextPlayer.inventory = previousPlayer->inventory;
        }
        // No pointers, projectiles, requests or NPC state survive replacement of the world.
        level = makeGameLevel(levelCatalog, levelNumber, atlasTextureId);
        startLevel(std::move(nextPlayer));
    }

    void ExampleGame::startLevel(Actor player)
    {
        placeFeetAt(player.body.bounds, level.playerSpawnFeet);
        const ActorId playerId = level.world.addActor(std::move(player));
        level.world.setPlayer(playerId, level.playerSpawnFeet);
        validateLevelActors(level.map, level.world, level.number);

        const Actor* playerActor = level.world.findActor(playerId);
        if (playerActor == nullptr)
        {
            throw std::logic_error("The example game could not initialise its camera");
        }
        cameraController =
            makeCameraController(level.map, playerActor->body.bounds, {80.0F, 45.0F});
    }

    void ExampleGame::update(const InputIntentions& intentions, float deltaTime)
    {
        if (gameComplete)
        {
            return;
        }
        Actor* player = level.world.findActor(level.world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The example game has no player");
        }

        player->intentions = intentions;
        updateWorldSimulation(level.map, level.world, deltaTime);

        if (level.world.levelComplete())
        {
            const auto& completedExit = level.world.exit();
            if (!completedExit.has_value())
            {
                throw std::logic_error("A completed game level must have an exit");
            }
            const auto nextLevel = completedExit->nextLevel;
            if (nextLevel.has_value())
            {
                loadLevel(*nextLevel);
            }
            else
            {
                gameComplete = true;
            }
            return;
        }

        player = level.world.findActor(level.world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The example game has no player after lifecycle update");
        }
        followTarget(cameraControllerValue(), level.map, player->body.bounds);
        updateWorldAnimations(level.world, deltaTime);
    }

    glm::vec2 ExampleGame::playerAimDirection(glm::vec2 screenPosition) const
    {
        const Actor* player = level.world.findActor(level.world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The example game has no player");
        }

        return screenToWorld(currentCamera(), screenPosition) - centerOf(player->body.bounds);
    }

    RenderScene ExampleGame::buildScene() const
    {
        const Actor* player = level.world.findActor(level.world.playerId());
        if (player == nullptr || !player->sprite.has_value())
        {
            throw std::logic_error("The example player is missing its sprite");
        }

        return buildRenderScene(
            level.map, player->sprite.value().textureId, currentCamera(), level.world);
    }

    DebugOverlay ExampleGame::debugOverlay() const
    {
        constexpr float AtlasWidth = 160.0F;
        return makeDebugOverlay(level.world, level.map, cameraControllerValue(), AtlasWidth);
    }

    Health ExampleGame::playerHealth() const
    {
        const Actor* player = level.world.findActor(level.world.playerId());
        if (player == nullptr || !player->health.has_value())
        {
            throw std::logic_error("The example player is missing its health");
        }
        return *player->health;
    }

    Camera ExampleGame::currentCamera() const
    {
        return cameraControllerValue().camera;
    }

    const Inventory& ExampleGame::playerInventory() const
    {
        const Actor* player = level.world.findActor(level.world.playerId());
        if (player == nullptr || !player->inventory.has_value())
        {
            throw std::logic_error("The example player is missing its inventory");
        }
        return *player->inventory;
    }

    const ItemDefinition& ExampleGame::itemDefinition(int id) const
    {
        return level.world.itemDefinition(id);
    }

    void ExampleGame::useInventoryItem(std::size_t slot)
    {
        if (gameComplete)
        {
            return;
        }
        WorldRequests requests;
        requests.useItem(level.world.playerId(), slot);
        // UI requests are applied while paused without advancing movement, combat or timers.
        applyWorldRequests(level.world, requests);
    }

    void ExampleGame::restart()
    {
        gameComplete = false;
        level = makeGameLevel(levelCatalog, levelCatalog.startLevel, atlasTextureId);
        startLevel(makePlayer(levelCatalog, atlasTextureId));
    }

    int ExampleGame::levelNumber() const
    {
        return level.number;
    }

    bool ExampleGame::complete() const
    {
        return gameComplete;
    }

    std::optional<glm::vec2> ExampleGame::levelExitScreenPosition() const
    {
        const auto& levelExit = level.world.exit();
        if (!levelExit.has_value())
        {
            return std::nullopt;
        }
        const Aabb& bounds = levelExit.value().bounds;
        const glm::vec2 topCenter = {bounds.position.x + bounds.size.x * 0.5F, bounds.position.y};
        return worldToScreen(currentCamera(), topCenter);
    }

    bool ExampleGame::exitReady() const
    {
        const Actor* player = level.world.findActor(level.world.playerId());
        const auto& levelExit = level.world.exit();
        return player != nullptr && levelExit.has_value() &&
               exitUnlocked(levelExit.value(), *player);
    }

    CameraController& ExampleGame::cameraControllerValue()
    {
        if (!cameraController.has_value())
        {
            throw std::logic_error("The example game camera is not initialised");
        }
        return *cameraController;
    }

    const CameraController& ExampleGame::cameraControllerValue() const
    {
        if (!cameraController.has_value())
        {
            throw std::logic_error("The example game camera is not initialised");
        }
        return *cameraController;
    }
}
