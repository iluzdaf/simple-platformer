#pragma once

#include <optional>

#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    // How long an exit takes to open once the player enters it with what it needs. The
    // game pauses while it opens, then the level completes.
    constexpr float ExitOpenSeconds = 1.0F;

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
        std::optional<double> lastLockedTouchTimeSeconds;
        // World-clock time when the living player entered an unlocked exit. If consumeItem
        // is set, the requirement is consumed then. Completion follows ExitOpenSeconds later.
        std::optional<double> openedTimeSeconds;
    };

    class World;
    struct Actor;

    // Checks runtime values; World additionally checks the required item exists.
    void validateLevelExit(const LevelExit& exit);

    bool exitUnlocked(const LevelExit& exit, const Actor& actor);
    // Whether the exit has been entered and the level has yet to complete.
    bool exitOpening(const World& world);
    void updateLevelExit(World& world);
}
