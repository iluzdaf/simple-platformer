#pragma once

#include "simple_platformer/world/sight.hpp"

namespace simple_platformer
{
    class TileMap;
    class World;

    // How NPCs and pickups fade from the player's view by the fraction of their body in
    // cover. A 12 by 20 actor standing in one row of grass is 0.8 in cover, so it must be
    // hidden by then.
    constexpr CoverFade ScreenCoverFade{0.5F, 0.75F};

    // For a full change from shown to hidden, or back.
    constexpr float CoverFadeSeconds = 0.2F;

    // Moves each NPC's and pickup's screenVisibility towards what the player can see of it,
    // by at most a CoverFadeSeconds' worth per call. One not shown before adopts its target
    // at once. The player is left alone.
    void updateCoverFades(const TileMap& map, World& world, float deltaTime);
}
