#pragma once

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace tests
{
    // The first generated neighbour reached by the traversal, or a failure if the test map
    // did not produce one.
    inline const simple_platformer::NavigationNeighbor& neighborWith(
        const std::vector<simple_platformer::NavigationNeighbor>& neighbors,
        simple_platformer::Traversal traversal)
    {
        const auto neighbor = std::find_if(
            neighbors.begin(),
            neighbors.end(),
            [traversal](const simple_platformer::NavigationNeighbor& candidate)
            { return candidate.traversal == traversal; });
        if (neighbor == neighbors.end())
        {
            throw std::logic_error("The expected navigation neighbor was not generated");
        }
        return *neighbor;
    }

    // The generated neighbour to the cell by the traversal, or a failure if the test map
    // did not produce one.
    inline const simple_platformer::NavigationNeighbor& neighborWith(
        const std::vector<simple_platformer::NavigationNeighbor>& neighbors,
        simple_platformer::GridPosition destination,
        simple_platformer::Traversal traversal)
    {
        const auto neighbor = std::find_if(
            neighbors.begin(),
            neighbors.end(),
            [destination, traversal](const simple_platformer::NavigationNeighbor& candidate) {
                return candidate.destinationCell == destination && candidate.traversal == traversal;
            });
        if (neighbor == neighbors.end())
        {
            throw std::logic_error("The expected navigation neighbor was not generated");
        }
        return *neighbor;
    }
}
