#include "example_content.hpp"
#include "actor_catalog.hpp"
#include "actor_definition.hpp"
#include "example_items.hpp"
#include "level_catalog.hpp"
#include "example_level_data.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "tile_catalog.hpp"
#include <stdexcept>
#include <utility>
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/pickup.hpp"

namespace simple_platformer
{
    namespace
    {
        Pickup makePickup(const ExamplePickupPlacement& placement)
        {
            Pickup pickup;
            pickup.bounds.size = {16, 16};
            placeFeetAt(pickup.bounds, placement.spawnFeet);
            pickup.stack = placement.stack;
            return pickup;
        }
        LevelExit makeExit(int textureId, const ExampleExitPlacement& placement)
        {
            LevelExit exit;
            exit.bounds.size = {16, 32};
            placeFeetAt(exit.bounds, placement.spawnFeet);
            exit.requirement = placement.requirement;
            exit.consumeItem = placement.consumeItem;
            exit.nextLevel = placement.nextLevel;
            exit.sprite = Sprite{textureId, {{48, 216}, {16, 32}}, {16, 32}};
            return exit;
        }
    }

    GameLevel makeGameLevel(const LevelCatalog& catalog, int levelNumber, int textureId)
    {
        const auto path = levelPath(catalog, levelNumber);
        const ExampleLevelData data = loadExampleLevelData(path);
        const TileCatalog tiles = loadTileCatalog(catalog.levelDirectory / "tiles.json");
        const ActorCatalog actors = loadActorCatalog(catalog.levelDirectory / "actors.json");
        for (const auto& reference : data.actorReferences)
        {
            try
            {
                actorDefinition(actors, reference.second);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(
                    path.string() + ": " + reference.first + ": " + error.what());
            }
        }
        World world(makeExampleItems(textureId));
        for (const auto& placement : data.actors)
        {
            try
            {
                world.addActor(composeActor(
                    actorDefinition(actors, placement.definitionName),
                    textureId,
                    placement.spawnFeet,
                    placement.patrol));
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(
                    path.string() + ": actor '" + placement.definitionName + "': " + error.what());
            }
        }
        for (const auto& placement : data.pickups)
        {
            world.addPickup(makePickup(placement));
        }
        world.setExit(makeExit(textureId, data.exit));
        return {
            levelNumber,
            makeTileMap(data.mapRows, data.tileLegend, tiles),
            std::move(world),
            data.playerSpawnFeet};
    }

    Actor makePlayer(const LevelCatalog& levels, int textureId)
    {
        const auto actors = loadActorCatalog(levels.levelDirectory / "actors.json");
        return composeActor(actorDefinition(actors, actors.player), textureId);
    }
}
