#include "hud_catalog.hpp"
#include "content_diagnostics.hpp"
#include "content_json.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string_view>
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    namespace
    {
        void validateHudIcon(const SpriteRegion& region, std::string_view name)
        {
            if (!isFiniteNonNegative(region.position) || !isFinitePositive(region.size))
            {
                failJson(
                    {}, name, "expected a finite, non-negative atlas position and a positive size");
            }
        }

        SpriteRegion jsonHudIcon(
            const nlohmann::json& root,
            std::string_view name,
            std::string_view sourceName)
        {
            const auto& value = requiredJsonMember(root, name, sourceName, "root");
            checkJsonFields(value, {"position", "size"}, sourceName, name);
            return jsonSpriteRegion(value, sourceName, name);
        }
    }

    void validateHudIcons(const HudIcons& icons)
    {
        validateHudIcon(icons.fullHeart, "fullHeart");
        validateHudIcon(icons.emptyHeart, "emptyHeart");
        validateHudIcon(icons.bag, "bag");
    }

    HudIcons parseHudIcons(std::string_view text, std::string_view sourceName)
    {
        const auto root = parseContentRoot(text, sourceName);
        checkJsonFields(root, {"fullHeart", "emptyHeart", "bag"}, sourceName, "root");
        HudIcons icons;
        icons.fullHeart = jsonHudIcon(root, "fullHeart", sourceName);
        icons.emptyHeart = jsonHudIcon(root, "emptyHeart", sourceName);
        icons.bag = jsonHudIcon(root, "bag", sourceName);
        validateInFile(sourceName, [&] { validateHudIcons(icons); });
        return icons;
    }

    HudIcons loadHudIcons(const std::filesystem::path& path)
    {
        return parseHudIcons(loadContentText(path), path.string());
    }
}
