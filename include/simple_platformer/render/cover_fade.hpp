#pragma once

#include <optional>

#include <glm/vec2.hpp>

namespace simple_platformer
{
    class TileMap;
    class World;
    struct Aabb;

    // The share of the body's area over tiles that block sight, from 0 to 1.
    float fractionInCover(const TileMap& map, const Aabb& bounds);

    // The fractions of a body's area in cover between which it fades.
    struct CoverFade
    {
        float concealsAbove;
        float hidesAbove;
    };

    // How visible the target is to the viewer, from 1 for fully visible to 0 for hidden. The
    // viewer is where it sees from, and sees fully whatever it has a line of sight to.
    // Otherwise the fade decides. Walls alone never hide a target in the open. Without a
    // viewer, cover alone decides.
    float visibility(
        const TileMap& map,
        std::optional<glm::vec2> viewer,
        const Aabb& target,
        CoverFade fade);

    // How anything on screen, the player included, fades by the fraction of its body in
    // cover. With the game's 16-pixel tiles, a 12 by 20 actor standing in one row of grass
    // is 0.8 in cover, so it must be hidden by then.
    constexpr CoverFade ScreenCoverFade{0.5F, 0.75F};

    // For a full change from shown to hidden, or back.
    constexpr float CoverFadeSeconds = 0.2F;

    // How long after firing the player is shown exposed, whatever cover they are in.
    constexpr float ShotRevealSeconds = 1.0F;

    // Moves each actor's and pickup's screenVisibility towards its target by at most
    // deltaTime / CoverFadeSeconds. One not shown before adopts its target at once.
    // For NPCs and pickups the target is what the player can see of them. For the player it
    // is what the world can see of them: their own cover fade, or fully exposed while any
    // NPC can see them or for ShotRevealSeconds after they fire.
    void updateCoverFades(const TileMap& map, World& world, float deltaTime);
}
