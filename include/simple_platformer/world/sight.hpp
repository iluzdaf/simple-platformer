#pragma once

#include <glm/vec2.hpp>

namespace simple_platformer
{
    class TileMap;

    // Ignores the cover the line starts in, so a viewer in grass can see out of it.
    bool lineOfSight(const TileMap& map, glm::vec2 from, glm::vec2 to);
}
