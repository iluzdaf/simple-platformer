#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/input/input_program.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/navigation/platformer_connection_cache.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/navigation/traversal.hpp"

namespace simple_platformer
{
    class TileMap;
    class World;
    struct PlatformerMovementConfig;

    // The feet along a jump or a fall, replayed from the start feet with the real
    // movement code at the step the program was recorded for, so the drawn arc matches
    // the actor's movement. Empty for any other traversal, or without inputs.
    std::vector<glm::vec2> sampleAirborneProgram(
        const TileMap& map,
        glm::vec2 startFeet,
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

    // The cursor cell's cached connections and their footprint. A break inside the
    // footprint invalidates those connections.
    struct CursorCellDebugInfo
    {
        Aabb bounds;
        std::optional<Aabb> footprint;
        std::vector<CachedConnectionDebugInfo> connections;
    };

    // One standable cell as the connection cache sees it: its connection count when
    // cached, or no count when absent, such as after a break drops it.
    struct NavigationCellDebugInfo
    {
        Aabb bounds;
        std::optional<std::size_t> connections;
    };

    struct NamedNavigationProfile
    {
        std::string name;
        PlatformerTraversalProfile profile;
    };

    // What the overlay is asked to show of navigation: which cell the cursor is over, in
    // world coordinates; which profile, by an index that wraps; and names supplied by
    // whoever knows the actor definitions.
    struct NavigationDebugView
    {
        std::optional<glm::vec2> cursorWorld;
        std::size_t profileIndex = 0;
        std::vector<NamedNavigationProfile> namedProfiles;
    };

    // Standable cells in the requested view, per-profile totals, and cache-wide counts.
    // Absent when no platformer NPC profile is known.
    struct NavigationCacheDebugInfo
    {
        glm::vec2 bodySize = {0.0F, 0.0F};
        std::string actorName;
        std::size_t profileIndex = 0;
        std::size_t profileCount = 0;
        // For this profile: every cached cell, standable or not, and those with connections.
        std::size_t cachedCellCount = 0;
        std::size_t cellsConnected = 0;
        // Cells awaiting the initial fill or recaching after a break.
        std::size_t cellsPending = 0;
        // Walk lengths simulated once for this profile, including failed attempts.
        std::size_t cachedWalkCount = 0;
        // Over every profile since the level started.
        std::size_t breaksApplied = 0;
        std::size_t cellsDropped = 0;
        std::size_t connectionWritesSoFar = 0;
        std::vector<NavigationCellDebugInfo> cells;
        std::optional<CursorCellDebugInfo> cursorCell;
    };

    // Built from the map and the world's connection cache, without ImGui, so it can be
    // tested. The step is the one the world is simulated with, which is part of the
    // profile the cache keys on. When visibleBounds is present, only cells overlapping it
    // are included; the cache totals still describe the whole map.
    std::optional<NavigationCacheDebugInfo> makeNavigationCacheDebugInfo(
        const World& world,
        const TileMap& map,
        float simulationStepSeconds,
        const NavigationDebugView& view = {},
        std::optional<Aabb> visibleBounds = std::nullopt);
}
