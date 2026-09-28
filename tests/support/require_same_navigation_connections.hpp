#pragma once

#include <cstddef>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "simple_platformer/navigation/navigation_path.hpp"
#include "support/require_same_input_program.hpp"

namespace tests
{
    inline void requireSameNavigationConnections(
        const std::vector<simple_platformer::NavigationConnection>& left,
        const std::vector<simple_platformer::NavigationConnection>& right)
    {
        REQUIRE(left.size() == right.size());
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            REQUIRE(left[index].step.destinationCell == right[index].step.destinationCell);
            REQUIRE(left[index].step.traversal == right[index].step.traversal);
            REQUIRE(left[index].cost == right[index].cost);
            requireSameInputProgram(left[index].step.inputs, right[index].step.inputs);
        }
    }
}
