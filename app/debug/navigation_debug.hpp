#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/input/input_program.hpp"
#include "simple_platformer/math/aabb.hpp"
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
    struct ConnectionDebugInfo
    {
        glm::vec2 fromFeet = {0.0F, 0.0F};
        glm::vec2 toFeet = {0.0F, 0.0F};
        Traversal traversal = Traversal::Walk;
        int cost = 0;
        // Feet sampled along a jump or fall; empty for other traversals.
        std::vector<glm::vec2> sampledFeet;
    };

    // The cursor cell's connections and their footprint, from the world's connection
    // table. A break inside the footprint rebuilds those connections. Neither is present
    // before the table builds the profile.
    struct CursorCellDebugInfo
    {
        Aabb bounds;
        std::optional<Aabb> footprint;
        std::vector<ConnectionDebugInfo> connections;
    };

    // One cell where the body can rest. Its count includes connections from every usable
    // surface; no count means the profile has not been built yet.
    struct NavigationCellDebugInfo
    {
        Aabb bounds;
        std::optional<std::size_t> connections;
        // False for a cell a climber can only hold a wall or the ceiling in.
        bool standable = true;
    };

    struct NamedNavigationProfile
    {
        std::string name;
        PlatformerTraversalProfile profile;
    };

    // Selects a navigation view: a world-space cursor, a profile index that wraps,
    // and actor-definition names supplied by the application.
    struct NavigationDebugView
    {
        std::optional<glm::vec2> cursorWorld;
        std::size_t profileIndex = 0;
        std::vector<NamedNavigationProfile> namedProfiles;
    };

    // Cells the body can rest in within the requested view, and per-profile totals.
    // Absent when no platformer NPC profile is known.
    struct NavigationConnectionsDebugInfo
    {
        glm::vec2 bodySize = {0.0F, 0.0F};
        std::string actorName;
        std::size_t profileIndex = 0;
        std::size_t profileCount = 0;
        // Cells with connections within the requested view.
        std::size_t cellsConnected = 0;
        std::vector<NavigationCellDebugInfo> cells;
        std::optional<CursorCellDebugInfo> cursorCell;
    };

    // Builds plain diagnostics from the map and connection table. Only sampling the
    // cursor cell's jump and fall arcs runs movement; collecting counts does not.
    // simulationStepSeconds must match the world's step, because profiles include it.
    // visibleBounds limits both the cells listed and the connected-cell count.
    std::optional<NavigationConnectionsDebugInfo> makeNavigationConnectionsDebugInfo(
        const World& world,
        const TileMap& map,
        float simulationStepSeconds,
        const NavigationDebugView& view = {},
        std::optional<Aabb> visibleBounds = std::nullopt);
}
