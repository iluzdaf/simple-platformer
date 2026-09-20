#pragma once

#include <map>
#include <string>
#include <vector>

#include "simple_platformer/world/tile_map.hpp"

namespace tests
{
    // Use asciiMap when the tiles are scenery: the test needs somewhere to stand and
    // something to bump into, and does not care which tile provides it. When a tile's
    // properties are the subject, declare the definitions in the test itself and call
    // TileMap::fromAscii with an explicit legend, so the test states its own premise.
    //
    // Do not add a third tile here. Tests would start relying on properties they never
    // declared, and a passing result would no longer say which property caused it.

    // '.' is empty, passable and see-through. '#' is solid, blocking movement and sight
    // together; it stands in for an obstacle rather than for any tile in the game, which is
    // why it cannot show that the two properties are independent. The sprite region is a
    // placeholder large enough to pass validation, and no test asserts on it.
    inline std::vector<simple_platformer::TileDefinition> asciiDefinitions()
    {
        return {{false, false, {}}, {true, true, {{0.0F, 0.0F}, {1.0F, 1.0F}}}};
    }

    // The symbols that select those definitions.
    inline std::map<char, int> asciiLegend()
    {
        return {{'.', 0}, {'#', 1}};
    }

    // Builds a map from rows of '.' and '#'.
    inline simple_platformer::TileMap asciiMap(const std::vector<std::string>& rows)
    {
        return simple_platformer::TileMap::fromAscii(rows, asciiDefinitions(), asciiLegend());
    }
}
