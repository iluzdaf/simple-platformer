#include "navigation_debug.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/actor_navigation.hpp"
#include "simple_platformer/input/input_program.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/navigation/platformer_connection_table.hpp"
#include "simple_platformer/navigation/traversal.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        Aabb cellBounds(int tileSize, Cell cell)
        {
            const auto size = static_cast<float>(tileSize);
            return {
                {static_cast<float>(cell.x) * size, static_cast<float>(cell.y) * size},
                {size, size}};
        }

        CursorCellDebugInfo cursorCellDebugInfo(
            const TileMap& map,
            const PlatformerConnectionTable& table,
            const PlatformerTraversalProfile& profile,
            Cell cell)
        {
            const int tileSize = map.tileSize();
            CursorCellDebugInfo info;
            info.bounds = cellBounds(tileSize, cell);
            if (!table.isBuilt(profile))
            {
                return info;
            }
            const CellRange& footprint = table.footprint(cell, profile);
            const Aabb first = cellBounds(tileSize, footprint.first);
            const Aabb last = cellBounds(tileSize, footprint.last);
            info.footprint = Aabb{first.topLeft, last.topLeft + last.size - first.topLeft};
            for (const RouteConnection& connection : table.connections(cell, profile))
            {
                const glm::vec2 fromFeet = feetOf(
                    boundsAtSurface(tileSize, {cell, connection.sourceSurface}, profile.size));
                info.connections.push_back(
                    {fromFeet,
                     feetOf(boundsAtSurface(tileSize, connection.step.destination, profile.size)),
                     connection.step.traversal,
                     connection.cost,
                     sampleAirborneProgram(
                         map,
                         fromFeet,
                         profile.size,
                         profile.movement,
                         connection.step.traversal,
                         connection.step.inputs,
                         profile.stepSeconds)});
            }
            return info;
        }
    }

    std::vector<glm::vec2> sampleAirborneProgram(
        const TileMap& map,
        glm::vec2 startFeet,
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
        body.bounds = boxStandingOn(startFeet, bodySize);
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

    std::optional<NavigationConnectionsDebugInfo> makeNavigationConnectionsDebugInfo(
        const World& world,
        const TileMap& map,
        float simulationStepSeconds,
        const NavigationDebugView& view,
        std::optional<Aabb> visibleBounds)
    {
        std::vector<PlatformerTraversalProfile> profiles;
        for (const Actor& actor : world.actors())
        {
            if (!actor.pathFollower.has_value() || !actor.platformerMovement.has_value())
            {
                continue;
            }
            const PlatformerTraversalProfile candidate =
                platformerTraversalProfileFor(actor, simulationStepSeconds);
            const bool known = std::any_of(
                profiles.begin(),
                profiles.end(),
                [&candidate](const PlatformerTraversalProfile& profile)
                { return profile == candidate; });
            if (!known)
            {
                profiles.push_back(candidate);
            }
        }
        if (profiles.empty())
        {
            return std::nullopt;
        }
        const std::size_t shown = view.profileIndex % profiles.size();
        const PlatformerTraversalProfile& profile = profiles[shown];
        NavigationConnectionsDebugInfo info;
        info.bodySize = profile.size;
        for (const NamedNavigationProfile& named : view.namedProfiles)
        {
            if (named.profile == profile)
            {
                info.actorName = named.name;
                break;
            }
        }
        info.profileIndex = shown;
        info.profileCount = profiles.size();
        const PlatformerConnectionTable& table = world.platformerConnections();
        const bool built = table.isBuilt(profile);
        for (int row = 0; row < map.height(); ++row)
        {
            for (int column = 0; column < map.width(); ++column)
            {
                const Cell cell{column, row};
                const bool standable = canStandAt(map, cell, profile.size);
                if (!standable &&
                    !(profile.climb.has_value() && canClimbAt(map, cell, profile.size)))
                {
                    continue;
                }
                const Aabb bounds = cellBounds(map.tileSize(), cell);
                if (visibleBounds.has_value() && !overlaps(bounds, *visibleBounds))
                {
                    continue;
                }
                if (!built)
                {
                    info.cells.push_back({bounds, std::nullopt, standable});
                    continue;
                }
                const std::size_t connections = table.connections(cell, profile).size();
                info.cells.push_back({bounds, connections, standable});
                if (connections > 0)
                {
                    ++info.cellsConnected;
                }
            }
        }
        if (view.cursorWorld.has_value())
        {
            const glm::vec2 cursor = view.cursorWorld.value_or(glm::vec2{0.0F, 0.0F});
            if (cursor.x >= 0.0F && cursor.y >= 0.0F && cursor.x < map.pixelWidth() &&
                cursor.y < map.pixelHeight())
            {
                info.cursorCell =
                    cursorCellDebugInfo(map, table, profile, cellAt(map.tileSize(), cursor));
            }
        }
        return info;
    }
}
