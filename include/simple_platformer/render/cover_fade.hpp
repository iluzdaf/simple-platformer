#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;

    // For a full change from shown to hidden, or back.
    constexpr float CoverFadeSeconds = 0.2F;

    // How long after firing the player is shown exposed, whatever cover they are in.
    constexpr float ShotRevealSeconds = 1.0F;

    // Moves each actor's and pickup's screenVisibility towards its target by at most
    // deltaTime / CoverFadeSeconds. One not shown before adopts its target at once.
    // Anything at most half in cover is shown fully, and anything three quarters in cover
    // is hidden, fading between; whatever the player has a line of sight to is shown fully.
    // For NPCs and pickups the target is what the player can see of them. For the player it
    // is what the world can see of them: their own cover alone, or fully exposed while any
    // NPC can see them or for ShotRevealSeconds after they fire.
    void updateCoverFades(const TileMap& map, World& world, float deltaTime);
}
