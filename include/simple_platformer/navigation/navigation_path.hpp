#pragma once

#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/input_program.hpp"

namespace simple_platformer
{
    // How a connection is travelled. Fly is the one kind a flying actor uses; the rest
    // are a platformer's.
    enum class Traversal
    {
        Fly,
        Walk,
        Fall,
        Jump
    };

    // One connection leaving a cell, as a neighbour policy reports it to the search.
    struct NavigationNeighbor
    {
        GridPosition destinationCell;
        Traversal traversal = Traversal::Fly;
        // Cost must be greater than zero. All connections in one search must
        // measure cost in the same unit, such as grid steps or simulation ticks.
        int cost = 1;
        // Recorded while the connection was simulated; empty for a walk or a flight.
        InputProgram inputs;
    };

    // One connection of a path: the cell it ends in, how it is travelled, and for a jump
    // or a fall the inputs that get there.
    struct NavigationStep
    {
        GridPosition destinationCell;
        Traversal traversal = Traversal::Fly;
        InputProgram inputs;
    };

    // Where a path begins and the connections that lead from there to its goal, in the
    // order travelled. No steps means the start is the goal.
    struct NavigationPath
    {
        GridPosition start;
        std::vector<NavigationStep> steps;
    };
}
