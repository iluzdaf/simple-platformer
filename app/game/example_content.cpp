#include "example_content.hpp"
#include "actor_catalog.hpp"
#include "actor_definition.hpp"
#include "item_catalog.hpp"
#include "pickup_catalog.hpp"
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
        Pickup makePickup(
            const ExamplePickupPlacement& placement,
            const PickupCatalog& pickups,
            const ItemCatalog& items,
            int textureId)
        {
            if (!placement.definitionName.empty())
            {
                return composePickup(
                    pickupDefinition(pickups, placement.definitionName),
                    items,
                    textureId,
                    placement.spawnFeet);
            }
            PickupDefinition definition;
            definition.stack = placement.stack;
            return composePickup(definition, items, textureId, placement.spawnFeet);
        }
        LevelExit makeExit(
            int textureId,
            const ExampleExitPlacement& placement,
            const ItemCatalog& items)
        {
            LevelExit exit;
            exit.bounds.size = {16, 32};
            placeFeetAt(exit.bounds, placement.spawnFeet);
            if (placement.requirement)
            {
                exit.requirement = resolveItemStack(items, *placement.requirement);
            }
            exit.consumeItem = placement.consumeItem;
            exit.nextLevel = placement.nextLevel;
            exit.sprite = Sprite{textureId, {{48, 216}, {16, 32}}, {16, 32}};
            return exit;
        }
    }

    GameLevel makeGameLevel(const LevelCatalog& catalog, int levelNumber, int textureId)
    {
        return makeGameLevel(
            catalog,
            levelNumber,
            textureId,
            loadItemCatalog(catalog.levelDirectory / "items.json"));
    }

    GameLevel makeGameLevel(
        const LevelCatalog& catalog,
        int levelNumber,
        int textureId,
        const ItemCatalog& items)
    {
        const auto path = levelPath(catalog, levelNumber);
        const ExampleLevelData data = loadExampleLevelData(path);
        const TileCatalog tiles = loadTileCatalog(catalog.levelDirectory / "tiles.json");
        const ActorCatalog actors = loadActorCatalog(catalog.levelDirectory / "actors.json");
        const PickupCatalog pickups =
            loadPickupCatalog(catalog.levelDirectory / "pickups.json", items);
        for (const auto& reference : data.itemReferences)
        {
            try
            {
                itemDefinition(items, reference.second);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(
                    path.string() + ": " + reference.first + ": " + error.what());
            }
        }
        for (const auto& reference : data.pickupReferences)
        {
            try
            {
                pickupDefinition(pickups, reference.second);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(
                    path.string() + ": " + reference.first + ": " + error.what());
            }
        }
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
        World world(composeItems(items, textureId));
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
            world.addPickup(makePickup(placement, pickups, items, textureId));
        }
        world.setExit(makeExit(textureId, data.exit, items));
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
