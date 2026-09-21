#pragma once

#include <optional>

#include <glm/vec2.hpp>

namespace simple_platformer
{
    class TileMap;
    struct Aabb;

    // Ignores the cover the line starts in, so a viewer in grass can see out of it.
    bool lineOfSight(const TileMap& map, glm::vec2 from, glm::vec2 to);

    // Judged by the centre of the bounds.
    bool standsInCover(const TileMap& map, const Aabb& bounds);

    // The viewer is where it sees from. Walls alone never hide a target in the open. Without
    // a viewer, everything in cover is hidden.
    bool hiddenByCover(const TileMap& map, std::optional<glm::vec2> viewer, const Aabb& target);
}
