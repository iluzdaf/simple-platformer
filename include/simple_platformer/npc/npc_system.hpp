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

    // Keeps the connections leaving every cell of the map for each platformer NPC body in
    // the world, at the step the NPCs will be simulated with, so the first chase of a
    // level does not simulate them during play. Call once when the level starts.
    void warmNpcNavigation(const TileMap& map, World& world, float stepSeconds);
}
