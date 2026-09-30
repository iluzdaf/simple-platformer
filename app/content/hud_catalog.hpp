#pragma once
#include <filesystem>
#include <string_view>
#include <glm/vec2.hpp>
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    // Where the HUD's icons sit in the atlas. The HUD lays each out at its own icon size.
    struct HudIcons
    {
        SpriteRegion fullHeart;
        SpriteRegion emptyHeart;
        SpriteRegion bag;
    };

    void validateHudIcons(const HudIcons& icons);
    HudIcons parseHudIcons(std::string_view text, std::string_view sourceName);
    HudIcons loadHudIcons(const std::filesystem::path& path);
    // Rejects the first HUD icon that runs past an atlas of this size, naming its field.
    void validateHudAtlasRegions(
        const HudIcons& icons,
        glm::ivec2 atlasSize,
        std::string_view sourceName);
}
