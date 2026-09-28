#pragma once

#include <optional>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/input/input_program.hpp"

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

    // One connection of a path: the cell it ends in, how it is travelled, and for a jump
    // or a fall the inputs that get there.
    struct NavigationStep
    {
        GridPosition destinationCell;
        Traversal traversal = Traversal::Fly;
        // Replay inputs for a jump or fall; empty for a walk or flight.
        InputProgram inputs;
    };

    // A traversable edge leaving a cell. Search uses its cost; a selected path keeps
    // its step. A neighboring cell alone is not a connection.
    struct NavigationConnection
    {
        NavigationStep step;
        // Cost must be greater than zero. All connections in one search must
        // measure cost in the same unit, such as grid steps or simulation ticks.
        int cost = 1;
    };

    // Where a path begins and the connections that lead from there to its goal, in the
    // order travelled. No steps means the start is the goal.
    struct NavigationPath
    {
        GridPosition start;
        std::vector<NavigationStep> steps;
    };

    enum class NavigationPathStatus
    {
        Found,
        Unreachable,
        Deferred
    };

    // Only Found carries a path. Deferred means the caller should retry after
    // pending navigation work completes; flying paths never defer.
    struct NavigationPathResult
    {
        NavigationPathStatus status = NavigationPathStatus::Unreachable;
        std::optional<NavigationPath> path;
    };
}
