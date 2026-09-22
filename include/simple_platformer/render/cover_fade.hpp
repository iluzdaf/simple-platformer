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

    // How long after firing the player is shown exposed, whatever cover they are in.
    constexpr float ShotRevealSeconds = 1.0F;

    // Moves each actor's and pickup's screenVisibility towards its target by at most a
    // CoverFadeSeconds' worth per call. One not shown before adopts its target at once.
    // For NPCs and pickups the target is what the player can see of them. For the player it
    // is what the world can see of them: their own cover fade, or fully exposed while any
    // NPC can see them or for ShotRevealSeconds after they fire.
    void updateCoverFades(const TileMap& map, World& world, float deltaTime);
}
