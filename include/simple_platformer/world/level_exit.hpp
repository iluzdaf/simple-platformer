#pragma once

#include <optional>

#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    struct LevelExit
    {
        Aabb bounds;
        std::optional<ItemStack> requirement;
        bool consumeItem = false;
        // Empty means the game is complete. The game resolves a level ID to level content.
        std::optional<int> nextLevel;
        std::optional<Sprite> sprite;
        // When the living player last stood in the exit without meeting its requirement, on
        // the world clock. The screen hints at what is missing for a while after.
        std::optional<float> lastLockedTouchTimeSeconds;
    };

    class World;
    struct Actor;

    // Checks runtime values; World additionally checks the required item exists.
    void validateLevelExit(const LevelExit& exit);

    bool exitUnlocked(const LevelExit& exit, const Actor& actor);
    void updateLevelExit(World& world);
}
