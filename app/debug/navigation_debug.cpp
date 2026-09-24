#include "navigation_debug.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/input_program.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        Aabb cellBounds(int tileSize, GridPosition cell)
        {
            const auto size = static_cast<float>(tileSize);
            return {
                {static_cast<float>(cell.x) * size, static_cast<float>(cell.y) * size},
                {size, size}};
        }

        // What the cache keeps for the cell under the cursor.
        CursorCellDebugInfo cursorCellDebugInfo(
            const TileMap& map,
            const PlatformerConnectionCache& cache,
            const ConnectionBody& body,
            GridPosition cell)
        {
            const int tileSize = map.tileSize();
            CursorCellDebugInfo info;
            info.bounds = cellBounds(tileSize, cell);
            const std::optional<CellRange> footprint = cache.footprintKept(cell, body);
            if (footprint.has_value())
            {
                const CellRange range = footprint.value_or(CellRange{});
                const Aabb first = cellBounds(tileSize, range.first);
                const Aabb last = cellBounds(tileSize, range.last);
                info.footprint = Aabb{first.position, last.position + last.size - first.position};
            }
            const std::vector<NavigationNeighbor>* kept = cache.find(cell, body);
            if (kept != nullptr)
            {
                for (const NavigationNeighbor& neighbor : *kept)
                {
                    info.connections.push_back(
                        {feetInCell(tileSize, cell),
                         feetInCell(tileSize, neighbor.destinationCell),
                         neighbor.traversal,
                         neighbor.cost,
                         sampleAirborneProgram(
                             map,
                             cell,
                             body.size,
                             body.movement,
                             neighbor.traversal,
                             neighbor.inputs,
                             body.stepSeconds)});
                }
            }
            const std::vector<GridPosition>* reachable = cache.reachableFrom(cell, body);
            if (reachable != nullptr)
            {
                for (const GridPosition reached : *reachable)
                {
                    info.reachable.push_back(cellBounds(tileSize, reached));
                }
            }
            return info;
        }
    }

    std::vector<glm::vec2> sampleAirborneProgram(
        const TileMap& map,
        GridPosition start,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        Traversal traversal,
        const InputProgram& inputs,
        float stepSeconds)
    {
        if ((traversal != Traversal::Jump && traversal != Traversal::Fall) || inputs.empty())
        {
            return {};
        }
        Body body;
        body.bounds = boxInCell(map.tileSize(), start, bodySize);
        PlatformerMovement flight{movement, true, 0.0F, 0.0F};
        std::vector<glm::vec2> sampledFeet;
        sampledFeet.push_back(feetOf(body.bounds));
        for (const InputStep& input : inputs)
        {
            const long ticks = std::lround(input.duration / stepSeconds);
            for (long tick = 0; tick < ticks; ++tick)
            {
                updatePlatformerMovement(map, body, flight, input.intentions, stepSeconds);
                sampledFeet.push_back(feetOf(body.bounds));
            }
        }
        return sampledFeet;
    }

    std::optional<NavigationCacheDebugInfo> makeNavigationCacheDebugInfo(
        const World& world,
        const TileMap& map,
        float simulationStepSeconds,
        std::optional<glm::vec2> cursorWorld,
        std::size_t bodyIndex)
    {
        std::vector<ConnectionBody> bodies;
        for (const Actor& actor : world.actors())
        {
            if (!actor.pathFollower.has_value() || !actor.platformerMovement.has_value())
            {
                continue;
            }
            const ConnectionBody candidate{
                actor.body.bounds.size,
                actor.platformerMovement.value().config,
                simulationStepSeconds};
            const bool known = std::any_of(
                bodies.begin(),
                bodies.end(),
                [&candidate](const ConnectionBody& body) { return body == candidate; });
            if (!known)
            {
                bodies.push_back(candidate);
            }
        }
        if (bodies.empty())
        {
            return std::nullopt;
        }
        const std::size_t shown = bodyIndex % bodies.size();
        const ConnectionBody body = bodies[shown];
        const PlatformerConnectionCache& cache = world.platformerConnections();
        NavigationCacheDebugInfo info;
        info.bodySize = body.size;
        info.bodyIndex = shown;
        info.bodyCount = bodies.size();
        info.cellsKept = cache.cellsKept(body);
        info.reachableSetsKept = cache.reachableSetsKept(body);
        info.pathsKept = cache.pathsKept(body);
        info.breaksApplied = cache.breaksApplied();
        info.cellsDropped = cache.cellsDroppedSoFar();
        info.cellsKeptSoFar = cache.cellsKeptSoFar();
        for (int row = 0; row < map.height(); ++row)
        {
            for (int column = 0; column < map.width(); ++column)
            {
                const GridPosition cell{column, row};
                if (!canStandAt(map, cell, body.size))
                {
                    continue;
                }
                const std::vector<NavigationNeighbor>* kept = cache.find(cell, body);
                info.cells.push_back(
                    {cellBounds(map.tileSize(), cell),
                     kept == nullptr ? std::nullopt : std::optional<std::size_t>(kept->size())});
            }
        }
        if (cursorWorld.has_value())
        {
            const glm::vec2 cursor = cursorWorld.value_or(glm::vec2{0.0F, 0.0F});
            if (cursor.x >= 0.0F && cursor.y >= 0.0F && cursor.x < map.pixelWidth() &&
                cursor.y < map.pixelHeight())
            {
                info.cursorCell =
                    cursorCellDebugInfo(map, cache, body, worldToGrid(map.tileSize(), cursor));
            }
        }
        return info;
    }
}
