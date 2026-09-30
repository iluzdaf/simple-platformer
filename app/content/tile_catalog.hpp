#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <glm/vec2.hpp>
#include <vector>

#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    struct TileCatalog
    {
        // The side of one tile in world pixels. Every nonempty tile's sprite region is this
        // size, because a tile is drawn into exactly one cell.
        int tileSize = 0;
        // Tile IDs are indices into definitions; ID 0 is reserved for empty.
        std::vector<TileDefinition> definitions;
        std::map<std::string, int> ids;
    };

    TileCatalog parseTileCatalog(std::string_view text, std::string_view sourceName);
    TileCatalog loadTileCatalog(const std::filesystem::path& path);
    // Rejects the first tile sprite that runs past an atlas of this size, naming its field.
    void validateTileAtlasRegions(
        const TileCatalog& catalog,
        glm::ivec2 atlasSize,
        std::string_view sourceName);
    // Resolves each map symbol through its catalog name to a runtime tile ID.
    TileMap composeTileMap(
        const std::vector<std::string>& rows,
        const std::map<char, std::string>& legend,
        const TileCatalog& catalog);
}
