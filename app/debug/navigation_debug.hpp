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
        // The arc of a jump or a fall; empty for a walk.
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

    // One cell the body can rest in, as the connection table holds it: its connection
    // count, or no count before the table builds the profile. The count covers every
    // place in the cell: its floor, and its walls and ceiling for a climber.
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

    // What the overlay is asked to show of navigation: which cell the cursor is over, in
    // world coordinates; which profile, by an index that wraps; and names supplied by
    // whoever knows the actor definitions.
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

    // Built from the map and the world's connection table, without ImGui, so it can be
    // tested. It never simulates movement except to sample the cursor cell's jump arcs.
    // The step is the one the world is simulated with, which is part of the profile the
    // table keys on. When visibleBounds is present, only cells overlapping it are included.
    std::optional<NavigationConnectionsDebugInfo> makeNavigationConnectionsDebugInfo(
        const World& world,
        const TileMap& map,
        float simulationStepSeconds,
        const NavigationDebugView& view = {},
        std::optional<Aabb> visibleBounds = std::nullopt);
}
