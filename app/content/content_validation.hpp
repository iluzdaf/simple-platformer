#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "tile_catalog.hpp"
#include "level_data.hpp"
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
    // These validators throw invalid_argument. A JSON loader passes sourceName so the
    // filename appears in the message; C++ callers leave it off. Unlike content_json,
    // sourceName is the trailing argument here, so existing path-only calls keep working.

    // Checks positive quantity, not spatial placement or whether the item exists.
    void validatePickupSettings(
        const PickupPlacement& placement,
        const std::string& path = "pickup",
        std::string_view sourceName = {});
    // Checks a nonempty definition name, positive requirement quantity and next-level number.
    // Does not check spatial placement or whether the item or target level exists.
    void validateExitSettings(
        const ExitPlacement& placement,
        const std::string& path = "exit",
        std::string_view sourceName = {});
    // Origins retain authoring locations for duplicate-placement diagnostics.
    void validateSinglePlacement(
        const std::vector<PlacementOrigin>& origins,
        std::string_view kind,
        std::string_view sourceName = {});
    void validateTileCatalog(const TileCatalog& catalog);
    void validateTileLegend(const std::map<char, std::string>& legend, const TileCatalog& catalog);
    // Keep unvalidated symbols as strings so entries such as "ZZ" can be rejected.
    void validateLegendSymbols(
        const std::vector<std::string>& tileSymbols,
        const std::vector<std::string>& objectSymbols,
        std::string_view sourceName = {});
    // The expanded legend includes object markers mapped to "empty" terrain.
    void validateMapRows(
        const std::vector<std::string>& rows,
        const std::map<char, std::string>& legend,
        std::string_view sourceName = {});
}
