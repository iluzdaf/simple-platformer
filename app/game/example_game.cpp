#include "example_game.hpp"

#include "debug/debug_overlay.hpp"
#include "example_content.hpp"
#include "example_items.hpp"

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
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"
#include "simple_platformer/world/world_simulation.hpp"

namespace simple_platformer
{
    ExampleGame::ExampleGame(int textureId) : map(makeExampleLevel(1)), atlasTextureId(textureId)
    {
        loadLevel(1);
    }

    void ExampleGame::loadLevel(int level)
    {
        if (level != 1 && level != 2)
        {
            throw std::invalid_argument("Unknown example level");
        }
        Actor nextPlayer = makeExamplePlayer(atlasTextureId);
        if (const Actor* previousPlayer = world.findActor(world.playerId()))
        {
            nextPlayer.health = previousPlayer->health;
            nextPlayer.inventory = previousPlayer->inventory;
        }
        // No pointers, projectiles, requests or NPC state survive replacement of the world.
        map = makeExampleLevel(level);
        world = World(makeExampleItems(atlasTextureId));
        currentLevel = level;
        const ActorId player = world.addActor(std::move(nextPlayer));
        world.setPlayer(player, {38.0F, 208.0F});
        populateExampleLevel(world, level, atlasTextureId);

        const Actor* playerActor = world.findActor(player);
        if (playerActor == nullptr)
        {
            throw std::logic_error("The example game could not initialise its camera");
        }
        cameraController = makeCameraController(map, playerActor->body.bounds, {80.0F, 45.0F});
    }

    void ExampleGame::update(const InputIntentions& intentions, float deltaTime)
    {
        if (gameComplete)
        {
            return;
        }
        Actor* player = world.findActor(world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The example game has no player");
        }

        player->intentions = intentions;
        updateWorldSimulation(map, world, deltaTime);

        if (world.levelComplete())
        {
            const auto& completedExit = world.exit();
            if (!completedExit.has_value())
            {
                throw std::logic_error("A completed example level must have an exit");
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

        player = world.findActor(world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The example game has no player after lifecycle update");
        }
        followTarget(cameraControllerValue(), map, player->body.bounds);
        updateActorAnimations(world, deltaTime);
    }

    glm::vec2 ExampleGame::playerAimDirection(glm::vec2 screenPosition) const
    {
        const Actor* player = world.findActor(world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The example game has no player");
        }

        return screenToWorld(currentCamera(), screenPosition) - centerOf(player->body.bounds);
    }

    RenderScene ExampleGame::buildScene() const
    {
        const Actor* player = world.findActor(world.playerId());
        if (player == nullptr || !player->sprite.has_value())
        {
            throw std::logic_error("The example player is missing its sprite");
        }

        return buildRenderScene(map, player->sprite.value().textureId, currentCamera(), world);
    }

    DebugOverlay ExampleGame::debugOverlay() const
    {
        constexpr float AtlasWidth = 160.0F;
        return makeDebugOverlay(world, map, cameraControllerValue(), AtlasWidth);
    }

    Health ExampleGame::playerHealth() const
    {
        const Actor* player = world.findActor(world.playerId());
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
        const Actor* player = world.findActor(world.playerId());
        if (player == nullptr || !player->inventory.has_value())
        {
            throw std::logic_error("The example player is missing its inventory");
        }
        return *player->inventory;
    }

    const ItemDefinition& ExampleGame::itemDefinition(int id) const
    {
        return world.itemDefinition(id);
    }

    void ExampleGame::useInventoryItem(std::size_t slot)
    {
        if (gameComplete)
        {
            return;
        }
        WorldRequests requests;
        requests.useItem(world.playerId(), slot);
        // UI requests are applied while paused without advancing movement, combat or timers.
        applyWorldRequests(world, requests);
    }

    void ExampleGame::restart()
    {
        world = World{};
        gameComplete = false;
        loadLevel(1);
    }

    int ExampleGame::levelNumber() const
    {
        return currentLevel;
    }

    bool ExampleGame::complete() const
    {
        return gameComplete;
    }

    std::optional<glm::vec2> ExampleGame::levelExitScreenPosition() const
    {
        const auto& levelExit = world.exit();
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
        const Actor* player = world.findActor(world.playerId());
        const auto& levelExit = world.exit();
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
