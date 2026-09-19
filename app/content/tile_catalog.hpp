#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    struct TileCatalog
    {
        // Tile IDs are indices into definitions; ID 0 is reserved for empty.
        std::vector<TileDefinition> definitions;
        std::map<std::string, int> ids;
    };

    TileCatalog parseTileCatalog(std::string_view text, std::string_view sourceName);
    TileCatalog loadTileCatalog(const std::filesystem::path& path);
    // Resolves each map symbol through its catalogue name to a runtime tile ID.
    TileMap composeTileMap(
        const std::vector<std::string>& rows,
        const std::map<char, std::string>& legend,
        const TileCatalog& catalog);
}
