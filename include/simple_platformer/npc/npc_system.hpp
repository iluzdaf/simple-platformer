#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;
    struct FrameProfile;
    // With a profile, counts the navigation searches the NPCs ran.
    void updateNpcBehaviour(
        const TileMap& map,
        World& world,
        float deltaTime,
        FrameProfile* profile = nullptr);
}
