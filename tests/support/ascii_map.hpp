#pragma once

#include <initializer_list>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "simple_platformer/world/tile_map.hpp"

namespace tests
{
    // The tile definitions behind asciiMap:
    //   '.' is empty, passable and see-through
    //   '#' is solid, blocking both movement and sight
    // The sprite region is a placeholder large enough to be valid. A test that asserts on
    // tile artwork, or that needs movement and sight to differ, should build its own
    // definitions and call TileMap::fromAscii with an explicit legend instead.
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
    inline simple_platformer::TileMap asciiMap(std::initializer_list<std::string_view> rows)
    {
        std::vector<std::string> ownedRows;
        ownedRows.reserve(rows.size());
        for (const std::string_view row : rows)
        {
            ownedRows.emplace_back(row);
        }
        return simple_platformer::TileMap::fromAscii(
            ownedRows, asciiDefinitions(), asciiLegend());
    }
}
