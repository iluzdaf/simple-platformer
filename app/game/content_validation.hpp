#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "tile_catalog.hpp"
#include "example_level_data.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    void validateContentSprite(const Sprite& sprite);
    // An authoring location for diagnostics, not a position in the game world.
    struct PlacementOrigin
    {
        std::string path;
        // Absent for explicit placements rather than map markers.
        std::optional<char> marker;
    };

    // Authoring rules shared by JSON loading and C++ content construction.
    // These validators throw invalid_argument; loaders add the source filename.

    // Checks positive quantity, not spatial placement or whether the item exists.
    void validatePickupSettings(
        const ExamplePickupPlacement& placement,
        const std::string& path = "pickup");
    // Checks a nonempty definition name, positive requirement quantity and next-level number.
    // Does not check spatial placement or whether the item or target level exists.
    void validateExitSettings(
        const ExampleExitPlacement& placement,
        const std::string& path = "exit");
    // Origins retain authoring locations for duplicate-placement diagnostics.
    void validateSinglePlacement(
        const std::vector<PlacementOrigin>& origins,
        std::string_view kind);
    void validateTileCatalog(const TileCatalog& catalog);
    void validateTileLegend(const std::map<char, std::string>& legend, const TileCatalog& catalog);
    // Keep unvalidated symbols as strings so entries such as "ZZ" can be rejected.
    void validateLegendSymbols(
        const std::vector<std::string>& tileSymbols,
        const std::vector<std::string>& objectSymbols);
    // The expanded legend includes object markers mapped to "empty" terrain.
    void validateMapRows(
        const std::vector<std::string>& rows,
        const std::map<char, std::string>& legend);
}
