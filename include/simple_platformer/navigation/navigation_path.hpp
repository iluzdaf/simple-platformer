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
