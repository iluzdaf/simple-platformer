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

    // The one way into navigation. Searches from where the actor's body rests, with
    // the connections its movement and capabilities give it, for the cheapest path to
    // the cell holding the goal. A goal it cannot reach gives the path that ends in the
    // reachable cell nearest it; the result says how far that end is from the goal. No
    // result means the actor has nowhere to start from yet, as when
    // it is in the air, or has no movement to navigate with.
    //
    // A platformer's connections are simulated tick by tick at stepSeconds, the fixed
    // step it is moved with, so a predicted jump and the real one run the same physics,
    // and costs are counted in those ticks. The step must be finite and positive. The
    // search never simulates: it reads the cache, which the navigation fill builds. A
    // cell the cache does not hold yet is queued for the fill and moved to the front,
    // and the result is Deferred. An optional frame profile records search work.
    std::optional<NavigationPathResult> findActorPath(
        const TileMap& map,
        const Actor& actor,
        glm::vec2 goalFeet,
        float stepSeconds,
        PlatformerConnectionCache& cache,
        FrameProfile* frameProfile = nullptr);

    // The profile a platformer actor's connections are simulated and cached for. The
    // navigation fill uses it to fill the same cache entries a search reads.
    PlatformerTraversalProfile platformerTraversalProfileFor(const Actor& actor, float stepSeconds);
}
