#pragma once

#include "simple_platformer/navigation/platformer_navigation.hpp"

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

    // Queues every cell of the map for each platformer NPC body in the world, at the step
    // the NPCs will be simulated with, so the fill keeps them over the first steps of
    // the level rather than the level start simulating them all. Call once when the
    // level starts.
    void queueNpcNavigation(const TileMap& map, World& world, float stepSeconds);

    // The movement ticks one simulation step may spend keeping queued cells, shared out
    // evenly among the bodies with cells waiting: a few cells a step, so a level start
    // or a break costs a little on each of the steps that follow instead of everything
    // on one, however many bodies the level has. A cell is never split, so a body may
    // run one cell past its share.
    constexpr int NavigationFillTicksPerStep = 250;

    // Simulates and keeps some of the cells queued for each platformer NPC body in the
    // world, within the budget, and reports what it did over every body. Runs every
    // step before the NPCs think; a search that needs a cell it has not reached waits
    // for it. With a profile, adds the ticks it simulated.
    FillWork fillNpcNavigation(
        const TileMap& map,
        World& world,
        float stepSeconds,
        FrameProfile* profile = nullptr);
}
