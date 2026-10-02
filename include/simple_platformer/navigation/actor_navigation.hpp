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

    // The one way into navigation. Finds the cheapest path for the actor, from where its
    // body rests to the cell holding the goal, using only the moves the actor has. If it
    // cannot reach that cell, the result is Unreachable with no path. No result means
    // the actor has
    // nowhere to start from yet, such as when it is in the air, or no movement to
    // navigate with.
    //
    // A platformer's connections come from running its movement at stepSeconds, the
    // fixed step it moves at, so a planned jump and the real one behave the same. Their
    // costs are counted in those ticks, and the step must be finite and positive. The
    // search itself never runs movement: it reads connections from the table, after
    // preparing the actor's profile there. That rebuilds what recent breaks touched, and
    // builds the whole map for a profile the table has not met.
    std::optional<NavigationPathResult> findActorPath(
        const TileMap& map,
        const Actor& actor,
        glm::vec2 goalFeet,
        float stepSeconds,
        PlatformerConnectionTable& connections);

    // Everything a platformer actor's connections depend on: its body size, movement,
    // climbing and the step.
    PlatformerTraversalProfile platformerTraversalProfileFor(const Actor& actor, float stepSeconds);

    // Builds the world's connection table for every platformer NPC's profile, so no
    // search during play has to. Call once when the level starts.
    void prepareNavigation(const TileMap& map, World& world, float stepSeconds);
}
