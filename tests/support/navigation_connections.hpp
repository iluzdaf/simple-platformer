#pragma once

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_graph.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "support/require_same_input_program.hpp"

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
        simple_platformer::Cell destination,
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

    // The first generated jump landing above the row, or a failure if the test map did
    // not produce one.
    inline const simple_platformer::NavigationConnection& jumpUpFrom(
        const std::vector<simple_platformer::NavigationConnection>& connections,
        int row)
    {
        const auto jump = std::find_if(
            connections.begin(),
            connections.end(),
            [row](const simple_platformer::NavigationConnection& connection)
            {
                return connection.step.traversal == simple_platformer::Traversal::Jump &&
                       connection.step.destinationCell.y < row;
            });
        if (jump == connections.end())
        {
            throw std::logic_error("No jump lands above the row");
        }
        return *jump;
    }

    inline void requireSameNavigationConnections(
        const std::vector<simple_platformer::NavigationConnection>& left,
        const std::vector<simple_platformer::NavigationConnection>& right)
    {
        REQUIRE(left.size() == right.size());
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            REQUIRE(left[index].sourceSurface == right[index].sourceSurface);
            REQUIRE(left[index].step.destinationCell == right[index].step.destinationCell);
            REQUIRE(left[index].step.destinationSurface == right[index].step.destinationSurface);
            REQUIRE(left[index].step.traversal == right[index].step.traversal);
            REQUIRE(left[index].cost == right[index].cost);
            requireSameInputProgram(left[index].step.inputs, right[index].step.inputs);
        }
    }
}
