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

    // The movement ticks one simulation step may spend keeping again the cells a break
    // dropped: a few cells a step, so a break costs a little on each of the steps that
    // follow instead of everything on one. A cell is never split, so a step may run one
    // cell past it.
    constexpr int NavigationRefillTicksPerStep = 250;

    // Simulates and keeps again some of the cells a break dropped, for each platformer
    // NPC body in the world, within the budget. Runs every step before the NPCs think;
    // a search that needs a cell it has not reached waits for it. With a profile, adds
    // the ticks it simulated.
    void refillNpcNavigation(
        const TileMap& map,
        World& world,
        float stepSeconds,
        FrameProfile* profile = nullptr);
}
