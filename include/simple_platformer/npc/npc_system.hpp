#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;
    struct FrameProfile;

    // Chooses each NPC's state and the intentions that act on it, searching the world's
    // navigation for paths as needed. With a profile, counts the searches the NPCs ran.
    void updateNpcBehaviour(
        const TileMap& map,
        World& world,
        float deltaTime,
        FrameProfile* profile = nullptr);
}
