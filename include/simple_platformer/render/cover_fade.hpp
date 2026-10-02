#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;

    // Seconds for screen visibility to change fully from shown to hidden, or back.
    constexpr float CoverFadeSeconds = 0.2F;

    // How long after firing the player is shown exposed, whatever cover they are in.
    constexpr float ShotRevealSeconds = 1.0F;

    // Updates screenVisibility without changing sensing. The first update sets it
    // immediately; later updates change it by at most deltaTime / CoverFadeSeconds.
    // NPCs and pickups are fully visible up to half their body area in cover, hidden from
    // three quarters, and fade between. The player's line of sight overrides their cover.
    // The player's own visibility uses cover alone, raised to fully exposed while an NPC
    // sees them or for ShotRevealSeconds after firing. Rendering uses it as shade.
    void updateCoverFades(const TileMap& map, World& world, float deltaTime);
}
