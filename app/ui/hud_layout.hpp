#pragma once

namespace simple_platformer
{
    // In internal pixels; the viewport scales them to the window.
    constexpr float HudIconSize = 16.0F;
    constexpr float HudMargin = 4.0F;
    constexpr float HudGap = 2.0F;
    // What floats above the level exit, the locked hint or the completion message, ends
    // this far above the door.
    constexpr float GapAboveDoor = 4.0F;
}
