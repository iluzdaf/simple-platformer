#pragma once

#include <vector>

#include "simple_platformer/input/input_program.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/traversal.hpp"

namespace simple_platformer
{
    // The search's nodes and edges. They stay inside navigation; a caller receives a
    // NavigationPath of waypoints instead.

    // A cell and the surface the body rests on. ClimbSurface::None means the floor.
    struct RouteLocation
    {
        Cell cell;
        ClimbSurface surface = ClimbSurface::None;
    };

    constexpr bool operator==(RouteLocation left, RouteLocation right)
    {
        return left.cell == right.cell && left.surface == right.surface;
    }

    // One edge of a route: its destination, traversal, and any recorded inputs.
    struct RouteStep
    {
        RouteLocation destination;
        Traversal traversal = Traversal::Fly;
        // Replay inputs for a jump, fall, or climb; empty for a walk or flight.
        InputProgram inputs;
    };

    // One move from a cell's sourceSurface to the step's destination. It may cross several
    // cells. The search uses its cost; the route keeps its step.
    struct RouteConnection
    {
        RouteStep step;
        // Cost must be greater than zero. All connections in one search must
        // measure cost in the same unit, such as grid steps or simulation ticks.
        int cost = 1;
        // The surface in its cell that the edge leaves from.
        ClimbSurface sourceSurface = ClimbSurface::None;
    };

    // Where the route begins and the steps in travel order. No steps means the start is
    // already the end.
    struct Route
    {
        RouteLocation start;
        std::vector<RouteStep> steps;
    };

    // The last step's destination, or the start of a route without steps.
    inline RouteLocation endOf(const Route& route)
    {
        return route.steps.empty() ? route.start : route.steps.back().destination;
    }
}
