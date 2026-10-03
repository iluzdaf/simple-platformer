#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"

namespace simple_platformer
{
    struct Actor;
    class PlatformerConnectionTable;
    class TileMap;
    class World;

    // Finds the cheapest path to the cell holding goalFeet, using the actor's movement.
    // A flyer starts in its feet cell; a platformer starts where its body rests. Returns
    // Unreachable with no path if the goal cell cannot be reached. Returns no result if
    // there is no starting location or movement component.
    //
    // Platformer connections are simulated at stepSeconds, which must be finite, positive,
    // and match the actor's update step. The search reads them from the table. Preparing
    // the profile first rebuilds cells affected by breaks and builds any new profile.
    std::optional<NavigationPathResult> findActorPath(
        const TileMap& map,
        const Actor& actor,
        glm::vec2 goalFeet,
        float stepSeconds,
        PlatformerConnectionTable& connections);

    // Everything a platformer actor's connections depend on: its body size, movement,
    // climbing and the step.
    PlatformerTraversalProfile platformerTraversalProfileFor(const Actor& actor, float stepSeconds);

    // Builds the profiles of the world's platformer NPCs at level startup. Their later
    // searches can read the table without simulating connections again.
    void prepareNavigation(const TileMap& map, World& world, float stepSeconds);
}
