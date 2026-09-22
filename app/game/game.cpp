#include "game.hpp"

#include "debug/debug_overlay.hpp"
#include "level_composition.hpp"
#include "content/level_catalog.hpp"
#include "content/game_catalogs.hpp"

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
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/presentation.hpp"
#include "simple_platformer/render/render_scene.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/level_validation.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "simple_platformer/world/world_simulation.hpp"

namespace simple_platformer
{
    Game::Game(int textureId, LevelCatalog catalog)
        : levelCatalog(std::move(catalog)),
          catalogs(loadGameCatalogs(levelCatalog.levelDirectory)),
          level(composeGameLevel(levelCatalog, levelCatalog.startLevel, textureId, catalogs)),
          atlasTextureId(textureId)
    {
        startLevel(composePlayer(catalogs, atlasTextureId));
    }

    void Game::loadLevel(int levelNumber)
    {
        Actor nextPlayer = composePlayer(catalogs, atlasTextureId);
        if (const Actor* previousPlayer = level.world.findActor(level.world.playerId()))
        {
            nextPlayer.health = previousPlayer->health;
            nextPlayer.inventory = previousPlayer->inventory;
        }
        // No pointers, projectiles, requests or NPC state survive replacement of the world.
        level = composeGameLevel(levelCatalog, levelNumber, atlasTextureId, catalogs);
        startLevel(std::move(nextPlayer));
    }

    void Game::startLevel(Actor player)
    {
        placeFeetAt(player.body.bounds, level.playerSpawnFeet);
        const ActorId playerId = level.world.addActor(std::move(player));
        level.world.setPlayer(playerId, level.playerSpawnFeet);
        validateLevelActors(level.map, level.world, level.number);

        const Actor* playerActor = level.world.findActor(playerId);
        if (playerActor == nullptr)
        {
            throw std::logic_error("The game could not initialise its camera");
        }
        cameraController =
            makeCameraController(level.map, playerActor->body.bounds, {80.0F, 45.0F});
    }

    void Game::update(const InputIntentions& intentions, float deltaTime)
    {
        if (gameComplete)
        {
            return;
        }
        Actor* player = level.world.findActor(level.world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The game has no player");
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
            throw std::logic_error("The game has no player after lifecycle update");
        }
        followTarget(cameraControllerValue(), level.map, player->body.bounds);
        updateWorldPresentation(level.map, level.world, deltaTime);
    }

    glm::vec2 Game::playerAimDirection(glm::vec2 screenPosition) const
    {
        const Actor* player = level.world.findActor(level.world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The game has no player");
        }

        return screenToWorld(currentCamera(), screenPosition) - centerOf(player->body.bounds);
    }

    RenderScene Game::buildScene() const
    {
        const Actor* player = level.world.findActor(level.world.playerId());
        if (player == nullptr || !player->sprite.has_value())
        {
            throw std::logic_error("The player is missing its sprite");
        }

        return buildRenderScene(
            level.map, player->sprite.value().textureId, currentCamera(), level.world);
    }

    DebugOverlay Game::debugOverlay(float atlasWidth) const
    {
        return makeDebugOverlay(level.world, level.map, cameraControllerValue(), atlasWidth);
    }

    Health Game::playerHealth() const
    {
        const Actor* player = level.world.findActor(level.world.playerId());
        if (player == nullptr || !player->health.has_value())
        {
            throw std::logic_error("The player is missing its health");
        }
        return *player->health;
    }

    Camera Game::currentCamera() const
    {
        return cameraControllerValue().camera;
    }

    const Inventory& Game::playerInventory() const
    {
        const Actor* player = level.world.findActor(level.world.playerId());
        if (player == nullptr || !player->inventory.has_value())
        {
            throw std::logic_error("The player is missing its inventory");
        }
        return *player->inventory;
    }

    const ItemDefinition& Game::itemDefinition(int id) const
    {
        return level.world.itemDefinition(id);
    }

    void Game::useInventoryItem(std::size_t slot)
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

    void Game::restart()
    {
        gameComplete = false;
        level = composeGameLevel(levelCatalog, levelCatalog.startLevel, atlasTextureId, catalogs);
        startLevel(composePlayer(catalogs, atlasTextureId));
    }

    int Game::levelNumber() const
    {
        return level.number;
    }

    bool Game::complete() const
    {
        return gameComplete;
    }

    std::optional<glm::vec2> Game::levelExitScreenPosition() const
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

    std::optional<Sprite> Game::lockedExitHintIcon() const
    {
        const auto& levelExit = level.world.exit();
        if (!levelExit.has_value() || !levelExit->requirement.has_value() ||
            !levelExit->lastLockedTouchTimeSeconds.has_value())
        {
            return std::nullopt;
        }
        const float sinceTouch =
            level.world.simulationTimeSeconds() - *levelExit->lastLockedTouchTimeSeconds;
        if (sinceTouch > LockedExitHintSeconds)
        {
            return std::nullopt;
        }
        return level.world.itemDefinition(levelExit->requirement->item).icon;
    }

    CameraController& Game::cameraControllerValue()
    {
        if (!cameraController.has_value())
        {
            throw std::logic_error("The game camera is not initialised");
        }
        return *cameraController;
    }

    const CameraController& Game::cameraControllerValue() const
    {
        if (!cameraController.has_value())
        {
            throw std::logic_error("The game camera is not initialised");
        }
        return *cameraController;
    }
}
