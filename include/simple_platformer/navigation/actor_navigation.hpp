#pragma once

#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"

namespace simple_platformer
{
    struct Actor;
    class PlatformerConnectionCache;
    struct FrameProfile;
    class TileMap;

    // The one way into navigation. Finds the cheapest path for the actor, from where its
    // body rests to the cell holding the goal, using only the moves the actor has. If it
    // cannot reach that cell, the path leads as close to the goal as possible, and the
    // result says how far the path's end is from it. No result means the actor has
    // nowhere to start from yet, such as when it is in the air, or no movement to
    // navigate with.
    //
    // A platformer's connections come from running its movement at stepSeconds, the
    // fixed step it moves at, so a planned jump and the real one behave the same. Their
    // costs are counted in those ticks, and the step must be finite and positive. The
    // search itself never runs movement: it reads connections from the cache, which the
    // navigation fill builds. If the cache does not hold a cell the search needs yet,
    // that cell goes to the front of the fill and the result is Deferred. An optional
    // frame profile records the search's work.
    std::optional<NavigationPathResult> findActorPath(
        const TileMap& map,
        const Actor& actor,
        glm::vec2 goalFeet,
        float stepSeconds,
        PlatformerConnectionCache& cache,
        FrameProfile* frameProfile = nullptr);

    // Everything a platformer actor's connections depend on: its body size, movement,
    // climbing and the step. The navigation fill uses it to build the same cache entries
    // a search reads.
    PlatformerTraversalProfile platformerTraversalProfileFor(const Actor& actor, float stepSeconds);
}
