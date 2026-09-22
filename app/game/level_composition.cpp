#include "level_composition.hpp"
#include "content/game_catalogs.hpp"
#include "content/actor_catalog.hpp"
#include "content/actor_definition.hpp"
#include "content/item_catalog.hpp"
#include "content/pickup_catalog.hpp"
#include "content/exit_catalog.hpp"
#include "content/level_catalog.hpp"
#include "content/level_data.hpp"
#include "content/tile_catalog.hpp"
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

#include <glm/vec2.hpp>
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    namespace
    {
        glm::vec2 feetOf(const TileMap& map, const LevelPosition& position)
        {
            if (const auto* cell = std::get_if<GridPosition>(&position))
            {
                return feetInCell(map.tileSize(), *cell);
            }
            return std::get<glm::vec2>(position);
        }

        Pickup makePickup(
            const TileMap& map,
            const PickupPlacement& placement,
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
                    feetOf(map, placement.spawn));
            }
            PickupDefinition definition;
            definition.stack = placement.stack;
            definition.bodySize = placement.bodySize;
            return composePickup(definition, items, textureId, feetOf(map, placement.spawn));
        }

        std::optional<Patrol> makePatrol(
            const TileMap& map,
            const std::optional<PatrolPlacement>& placement)
        {
            if (!placement.has_value())
            {
                return std::nullopt;
            }
            return Patrol{feetOf(map, placement->first), feetOf(map, placement->second), true};
        }

        LevelExit makeExit(
            const TileMap& map,
            int textureId,
            const ExitPlacement& placement,
            const ItemCatalog& items,
            const ExitCatalog& exits)
        {
            LevelExit exit = composeExit(
                exitDefinition(exits, placement.definitionName),
                textureId,
                feetOf(map, placement.spawn));
            if (placement.requirement)
            {
                exit.requirement = composeItemStack(items, *placement.requirement);
            }
            exit.consumeItem = placement.consumeItem;
            exit.nextLevel = placement.nextLevel;
            return exit;
        }
    }

    GameLevel composeGameLevel(const LevelCatalog& catalog, int levelNumber, int textureId)
    {
        return composeGameLevel(
            catalog, levelNumber, textureId, loadGameCatalogs(catalog.levelDirectory));
    }

    GameLevel composeGameLevel(
        const LevelCatalog& catalog,
        int levelNumber,
        int textureId,
        const GameCatalogs& catalogs)
    {
        const auto path = levelPath(catalog, levelNumber);
        const LevelData data = loadLevelData(path);
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
        TileMap map = composeTileMap(data.mapRows, data.tileLegend, tiles);
        World world(composeItems(items, textureId));
        for (const auto& placement : data.actors)
        {
            try
            {
                world.addActor(composeActor(
                    actorDefinition(actors, placement.definitionName),
                    catalogs.animations,
                    textureId,
                    feetOf(map, placement.spawn),
                    makePatrol(map, placement.patrol)));
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument(
                    path.string() + ": actor '" + placement.definitionName + "': " + error.what());
            }
        }
        for (const auto& placement : data.pickups)
        {
            world.addPickup(makePickup(map, placement, pickups, items, textureId));
        }
        world.setExit(makeExit(map, textureId, data.exit, items, exits));
        const glm::vec2 playerSpawnFeet = feetOf(map, data.playerSpawn);
        return {levelNumber, std::move(map), std::move(world), playerSpawnFeet};
    }

    Actor composePlayer(const GameCatalogs& catalogs, int textureId)
    {
        const auto& actors = catalogs.actors;
        return composeActor(actorDefinition(actors, actors.player), catalogs.animations, textureId);
    }
}
