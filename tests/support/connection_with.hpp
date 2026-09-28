#pragma once

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace tests
{
    // The first generated connection using the traversal, or a failure if the test map
    // did not produce one.
    inline const simple_platformer::NavigationConnection& connectionWith(
        const std::vector<simple_platformer::NavigationConnection>& connections,
        simple_platformer::Traversal traversal)
    {
        const auto connection = std::find_if(
            connections.begin(),
            connections.end(),
            [traversal](const simple_platformer::NavigationConnection& candidate)
            { return candidate.step.traversal == traversal; });
        if (connection == connections.end())
        {
            throw std::logic_error("The expected navigation connection was not generated");
        }
        return *connection;
    }

    // The generated connection to the cell by the traversal, or a failure if the test map
    // did not produce one.
    inline const simple_platformer::NavigationConnection& connectionWith(
        const std::vector<simple_platformer::NavigationConnection>& connections,
        simple_platformer::GridPosition destination,
        simple_platformer::Traversal traversal)
    {
        const auto connection = std::find_if(
            connections.begin(),
            connections.end(),
            [destination, traversal](const simple_platformer::NavigationConnection& candidate) {
                return candidate.step.destinationCell == destination &&
                       candidate.step.traversal == traversal;
            });
        if (connection == connections.end())
        {
            throw std::logic_error("The expected navigation connection was not generated");
        }
        return *connection;
    }
}
