#include "game.hpp"

#include "debug/debug_overlay.hpp"
#include "content/actor_catalog.hpp"
#include "debug/navigation_debug.hpp"
#include "level_composition.hpp"
#include "content/level_catalog.hpp"
#include "content/game_catalogs.hpp"
#include "content/npc_script_catalog.hpp"

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
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/navigation/navigation_fill.hpp"
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
    Game::Game(int textureId, LevelCatalog catalog, float stepSeconds)
        : levelCatalog(std::move(catalog)),
          catalogs(loadGameCatalogs(levelCatalog.levelDirectory)),
          level(composeGameLevel(levelCatalog, levelCatalog.startLevel, textureId, catalogs)),
          atlasTextureId(textureId),
          simulationStepSeconds(stepSeconds)
    {
        if (!isFinitePositive(simulationStepSeconds))
        {
            throw std::invalid_argument("The game's simulation step must be finite and positive");
        }
        loadNpcActivityScripts(
            npcScripts, catalogs.machines, levelCatalog.levelDirectory / "scripts");
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
        for (const Actor& actor : level.world.actors())
        {
            npcScripts.forget(actor.id);
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
        queueNavigation(
            level.map,
            platformerBodiesIn(level.world, simulationStepSeconds),
            level.world.platformerConnections());
    }

    void Game::update(const InputIntentions& intentions, float deltaTime, FrameProfile* profile)
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
        updateWorldSimulation(level.map, level.world, deltaTime, profile, &npcScripts);

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

    DebugOverlay Game::debugOverlay(
        float atlasWidth,
        std::optional<glm::vec2> internalCursor,
        std::size_t navigationBodyIndex,
        std::optional<ActorId> lockedMachineActor) const
    {
        NavigationDebugView navigation;
        if (internalCursor.has_value())
        {
            navigation.cursorWorld =
                screenToWorld(currentCamera(), internalCursor.value_or(glm::vec2{0.0F, 0.0F}));
        }
        navigation.bodyIndex = navigationBodyIndex;
        // Every NPC definition that walks names the body the cache would key it by.
        for (const auto& [name, definition] : catalogs.actors.definitions)
        {
            if (definition.platformer.has_value() && definition.senses.has_value())
            {
                navigation.bodyNames.push_back(
                    {name,
                     {definition.bodySize,
                      definition.platformer.value_or(PlatformerMovementConfig{}),
                      simulationStepSeconds}});
            }
        }
        return makeDebugOverlay(
            level.world,
            level.map,
            cameraControllerValue(),
            atlasWidth,
            simulationStepSeconds,
            navigation,
            lockedMachineActor);
    }

    std::optional<ActorId> Game::machineActorAt(glm::vec2 internalPosition) const
    {
        const glm::vec2 worldPosition = screenToWorld(currentCamera(), internalPosition);
        for (const Actor& actor : level.world.actors())
        {
            if (actor.machine.has_value() && contains(actor.body.bounds, worldPosition))
            {
                return actor.id;
            }
        }
        return std::nullopt;
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

    bool Game::breakTileAt(glm::vec2 internalPosition)
    {
        const glm::vec2 world = screenToWorld(currentCamera(), internalPosition);
        if (world.x < 0.0F || world.y < 0.0F || world.x >= level.map.pixelWidth() ||
            world.y >= level.map.pixelHeight())
        {
            return false;
        }
        return level.map.breakTile(worldToGrid(level.map.tileSize(), world));
    }

    void Game::restart()
    {
        gameComplete = false;
        for (const Actor& actor : level.world.actors())
        {
            npcScripts.forget(actor.id);
        }
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
        if (!levelExit.has_value() || !levelExit->requirement.has_value())
        {
            return std::nullopt;
        }
        const std::optional<float> sinceTouch =
            level.world.secondsSince(levelExit->lastLockedTouchTimeSeconds);
        if (!sinceTouch.has_value() || *sinceTouch > LockedExitHintSeconds)
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
