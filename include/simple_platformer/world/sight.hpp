#pragma once

#include <optional>

#include <glm/vec2.hpp>

namespace simple_platformer
{
    class TileMap;
    struct Aabb;

    // Ignores the cover the line starts in, so a viewer in grass can see out of it.
    bool lineOfSight(const TileMap& map, glm::vec2 from, glm::vec2 to);

    // The share of the body's area over tiles that block sight, from 0 to 1.
    float fractionInCover(const TileMap& map, const Aabb& bounds);

    // The fractions of a body's area in cover between which it fades.
    struct CoverFade
    {
        float concealsAbove;
        float hidesAbove;
    };

    // How visible the target is to the viewer, from 1 for fully visible to 0 for hidden. The
    // viewer is where it sees from, and sees fully whatever it has a line of sight to.
    // Otherwise the fade decides. Walls alone never hide a target in the open. Without a
    // viewer, cover alone decides.
    float visibility(
        const TileMap& map,
        std::optional<glm::vec2> viewer,
        const Aabb& target,
        CoverFade fade);
}
