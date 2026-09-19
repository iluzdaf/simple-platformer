#include "example_content.hpp"
#include "game_catalogs.hpp"
#include "actor_catalog.hpp"
#include "actor_definition.hpp"
#include "item_catalog.hpp"
#include "pickup_catalog.hpp"
#include "exit_catalog.hpp"
#include "level_catalog.hpp"
#include "example_level_data.hpp"
#include "tile_catalog.hpp"
#include <stdexcept>
#include <utility>
#include "simple_platformer/actor/actor.hpp"
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
            const ItemCatalog& items,
            const ExitCatalog& exits)
        {
            LevelExit exit = composeExit(
                exitDefinition(exits, placement.definitionName), textureId, placement.spawnFeet);
            if (placement.requirement)
            {
                exit.requirement = resolveItemStack(items, *placement.requirement);
            }
            exit.consumeItem = placement.consumeItem;
            exit.nextLevel = placement.nextLevel;
            return exit;
        }
    }

    GameLevel makeGameLevel(const LevelCatalog& catalog, int levelNumber, int textureId)
    {
        return makeGameLevel(
            catalog, levelNumber, textureId, loadGameCatalogs(catalog.levelDirectory));
    }

    GameLevel makeGameLevel(
        const LevelCatalog& catalog,
        int levelNumber,
        int textureId,
        const GameCatalogs& catalogs)
    {
        const auto path = levelPath(catalog, levelNumber);
        const ExampleLevelData data = loadExampleLevelData(path);
        const auto& tiles = catalogs.tiles;
        const auto& actors = catalogs.actors;
        const auto& exits = catalogs.exits;
        const auto& items = catalogs.items;
        const auto& pickups = catalogs.pickups;
        for (const auto& reference : data.exitReferences)
        {
            try
            {
                exitDefinition(exits, reference.second);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(
                    path.string() + ": " + reference.first + ": " + error.what());
            }
        }
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
                    catalogs.animations,
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
        world.setExit(makeExit(textureId, data.exit, items, exits));
        return {
            levelNumber,
            makeTileMap(data.mapRows, data.tileLegend, tiles),
            std::move(world),
            data.playerSpawnFeet};
    }

    Actor makePlayer(const GameCatalogs& catalogs, int textureId)
    {
        const auto& actors = catalogs.actors;
        return composeActor(actorDefinition(actors, actors.player), catalogs.animations, textureId);
    }
}
