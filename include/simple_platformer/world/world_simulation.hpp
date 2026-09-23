#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;
    struct FrameProfile;

    // One fixed step of gameplay. With a profile, each phase's cost is added to it under a
    // short name for the step; without one, nothing is timed.
    void updateWorldSimulation(
        TileMap& map,
        World& world,
        float deltaTime,
        FrameProfile* profile = nullptr);
}
