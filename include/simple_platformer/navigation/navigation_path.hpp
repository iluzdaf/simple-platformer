#pragma once

#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/input_program.hpp"

namespace simple_platformer
{
    enum class Traversal
    {
        Fly,
        Walk,
        Fall,
        Jump
    };

    struct NavigationNeighbor
    {
        GridPosition destination;
        Traversal traversal = Traversal::Fly;
        // Cost must be greater than zero. All connections in one search must
        // measure cost in the same unit, such as grid steps or simulation ticks.
        int cost = 1;
        InputProgram inputs;
    };

    struct NavigationStep
    {
        GridPosition destination;
        Traversal traversal = Traversal::Fly;
        InputProgram inputs;
    };

    struct NavigationPath
    {
        GridPosition start;
        std::vector<NavigationStep> steps;
    };
}
