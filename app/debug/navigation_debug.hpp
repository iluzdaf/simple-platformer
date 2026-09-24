#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/input_program.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    class TileMap;
    class World;
    struct PlatformerMovementConfig;

    // The feet along a jump or a fall, replayed from the cell with the real movement code
    // at the step the program was recorded for, so the arc drawn is the arc the actor will
    // fly. Empty for any other traversal, or without inputs.
    std::vector<glm::vec2> sampleAirborneProgram(
        const TileMap& map,
        GridPosition start,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement,
        Traversal traversal,
        const InputProgram& inputs,
        float stepSeconds);

    // One connection leaving the cursor cell, resolved to feet for drawing.
    struct CachedConnectionDebugInfo
    {
        glm::vec2 fromFeet = {0.0F, 0.0F};
        glm::vec2 toFeet = {0.0F, 0.0F};
        Traversal traversal = Traversal::Walk;
        int cost = 0;
        // The arc of a jump or a fall; empty for a walk.
        std::vector<glm::vec2> sampledFeet;
    };

    // What the cache keeps for the cell under the cursor: the footprint its simulation
    // swept, which is why a break there drops it; its connections; and the cells a failed
    // search found reachable from it, when one has.
    struct CursorCellDebugInfo
    {
        Aabb bounds;
        std::optional<Aabb> footprint;
        std::vector<CachedConnectionDebugInfo> connections;
        std::vector<Aabb> reachable;
    };

    // One cell a body can stand in, as the connection cache sees it: how many connections
    // it keeps for the cell, or nothing while it keeps none, as after a break drops them.
    struct NavigationCellDebugInfo
    {
        Aabb bounds;
        std::optional<std::size_t> connections;
    };

    // A body as the cache keys it, with the name of the actor definition it came from.
    struct NamedBody
    {
        std::string name;
        ConnectionBody body;
    };

    // What the overlay is asked to show of navigation: which cell the cursor is over, in
    // world coordinates; which body, by an index that wraps; and the names to show bodies
    // by, from whoever knows the actor definitions.
    struct NavigationDebugView
    {
        std::optional<glm::vec2> cursorWorld;
        std::size_t bodyIndex = 0;
        std::vector<NamedBody> bodyNames;
    };

    // The connection cache's view of the map for one platformer NPC body: every cell that
    // body can stand in, and what the cache has kept and done so far. Absent without such
    // an NPC. The bodies are the distinct ones in the world, in the order first found.
    struct NavigationCacheDebugInfo
    {
        glm::vec2 bodySize = {0.0F, 0.0F};
        // Empty when no name was given for the body.
        std::string bodyName;
        std::size_t bodyIndex = 0;
        std::size_t bodyCount = 0;
        // For this body: every cell kept, standable or not, and those with connections.
        std::size_t cellsKept = 0;
        std::size_t cellsConnected = 0;
        std::size_t reachableSetsKept = 0;
        std::size_t pathsKept = 0;
        // Over every body since the level started.
        std::size_t breaksApplied = 0;
        std::size_t cellsDropped = 0;
        std::size_t cellsKeptSoFar = 0;
        std::vector<NavigationCellDebugInfo> cells;
        // Present while the cursor is over the map.
        std::optional<CursorCellDebugInfo> cursorCell;
    };

    // Built from the map and the world's connection cache, without ImGui, so it can be
    // tested. The step is the one the world is simulated with, which is part of the
    // body the cache keys on.
    std::optional<NavigationCacheDebugInfo> makeNavigationCacheDebugInfo(
        const World& world,
        const TileMap& map,
        float simulationStepSeconds,
        const NavigationDebugView& view = {});
}
