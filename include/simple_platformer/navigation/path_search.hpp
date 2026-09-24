#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    // Given each connection leaving a cell, with the cost the search charges for it: the
    // connection's own unless the policy adds to it, as a platformer search does to a jump.
    using GridNeighborVisitor = std::function<void(const NavigationNeighbor& neighbor, int cost)>;
    // A policy hands the search the connections leaving a cell by visiting each where it
    // is, so one that keeps them, such as a cache, need not copy them out for every search.
    using GridNeighborFunction =
        std::function<void(GridPosition cell, const GridNeighborVisitor& visit)>;
    // An optimistic guess at the cost left from a cell to the goal, never below zero.
    using GridHeuristicFunction = std::function<int(GridPosition cell, GridPosition goal)>;

    // What a search cost, for the frame profile.
    struct PathSearchStatistics
    {
        // Cells whose connections the search asked for.
        int nodesExpanded = 0;
        // Of those, cells whose connections a cache already held.
        int cellsReused = 0;
        // Searches answered with a path kept from an earlier one, expanding nothing.
        int pathsRemembered = 0;
        // Searches that met a cell a break dropped and the refill had not reached, found
        // no path without it, and gave up rather than simulate; the caller asks again.
        int deferred = 0;
        // Movement ticks simulated to build connections; only platformer searches do this.
        int simulatedTicks = 0;
    };

    int manhattanHeuristic(GridPosition cell, GridPosition goal);

    // The cheapest path from the start to the goal over the connections the policy
    // reports, in A* order: the heuristic must never overestimate the cost left, and with
    // a heuristic of zero the search expands in order of cost from the start alone. The
    // search runs over a grid of the given size and keeps a slot per cell of it, so a
    // connection's destination is found without hashing or scanning; the start, the goal
    // and every destination must lie within it. Only the connections the search follows
    // are copied, into the path. With statistics, counts the cells it expanded. When
    // there is no path and reached is given, fills it with every cell the search got to,
    // which is every cell reachable from the start; a search that finds a path leaves
    // it alone.
    std::optional<NavigationPath> findLowestCostPath(
        GridPosition start,
        GridPosition goal,
        GridSize grid,
        const GridNeighborFunction& neighbors,
        const GridHeuristicFunction& heuristic,
        PathSearchStatistics* statistics = nullptr,
        std::vector<GridPosition>* reached = nullptr);

    // The same search with a heuristic of zero, for comparison and for tests.
    std::optional<NavigationPath> findLowestCostPath(
        GridPosition start,
        GridPosition goal,
        GridSize grid,
        const GridNeighborFunction& neighbors);
}
