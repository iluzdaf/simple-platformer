#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include "simple_platformer/render/animation.hpp"

namespace simple_platformer
{
    using AnimationCatalog = std::map<std::string, AnimationSet>;
    // Actor selection can request any of the six clips, so each set supplies all six.
    void validateAnimationSet(const AnimationSet& set);
    void validateAnimationCatalog(const AnimationCatalog& catalog);
    AnimationCatalog parseAnimationCatalog(std::string_view text, std::string_view sourceName);
    AnimationCatalog loadAnimationCatalog(const std::filesystem::path& path);
    const AnimationSet& animationSet(const AnimationCatalog& catalog, const std::string& name);
}
