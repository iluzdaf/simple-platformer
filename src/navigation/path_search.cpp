#include "simple_platformer/navigation/path_search.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    namespace
    {
        // How a node was cheapest reached, for walking the path back to the start.
        struct IncomingStep
        {
            std::size_t parentIndex;
            NavigationStep step;
        };

        // A cell the search has reached. Closed once expanded, and opened again if a
        // cheaper way to it turns up.
        struct SearchNode
        {
            GridPosition cell;
            int costFromStart = 0;
            int estimatedTotalCost = 0;
            std::optional<IncomingStep> incoming;
            bool closed = false;
        };

        int estimateRemainingCost(
            const GridHeuristicFunction& heuristic,
            GridPosition cell,
            GridPosition goal)
        {
            const int estimate = heuristic(cell, goal);
            if (estimate < 0)
            {
                throw std::invalid_argument("A path heuristic cannot return a negative cost");
            }
            return estimate;
        }

        // Scanning preserves discovery order when estimated costs tie.
        std::optional<std::size_t> cheapestOpenNode(const std::vector<SearchNode>& nodes)
        {
            std::optional<std::size_t> cheapest;
            for (std::size_t index = 0; index < nodes.size(); ++index)
            {
                if (nodes[index].closed)
                {
                    continue;
                }
                if (!cheapest.has_value())
                {
                    cheapest = index;
                    continue;
                }
                if (nodes[index].estimatedTotalCost < nodes[cheapest.value()].estimatedTotalCost)
                {
                    cheapest = index;
                }
            }
            return cheapest;
        }

        NavigationPath reconstructPath(const std::vector<SearchNode>& nodes, std::size_t goalIndex)
        {
            std::vector<NavigationStep> steps;
            std::size_t current = goalIndex;
            while (true)
            {
                const SearchNode& currentNode = nodes[current];
                if (!currentNode.incoming.has_value())
                {
                    break;
                }

                const IncomingStep& incoming = currentNode.incoming.value();
                steps.push_back(incoming.step);
                current = incoming.parentIndex;
            }
            std::reverse(steps.begin(), steps.end());
            return {nodes[current].cell, std::move(steps)};
        }

    }

    int manhattanHeuristic(GridPosition cell, GridPosition goal)
    {
        return std::abs(cell.x - goal.x) + std::abs(cell.y - goal.y);
    }

    PathSearchResult findLowestCostPath(
        GridPosition start,
        GridPosition goal,
        GridSize grid,
        const GridNeighborFunction& neighbors,
        const GridConnectionFunction& connections,
        const GridHeuristicFunction& heuristic,
        const GridExpansionReady& canExpand)
    {
        if (!neighbors)
        {
            throw std::invalid_argument("Path search requires a neighbor function");
        }
        if (!connections)
        {
            throw std::invalid_argument("Path search requires a connection function");
        }
        if (!heuristic)
        {
            throw std::invalid_argument("Path search requires a heuristic function");
        }
        if (grid.width <= 0 || grid.height <= 0)
        {
            throw std::invalid_argument("Path search requires a grid with cells");
        }
        if (!contains(grid, start) || !contains(grid, goal))
        {
            throw std::invalid_argument("Path search start and goal must lie within the grid");
        }

        std::vector<SearchNode> nodes{
            {start, 0, estimateRemainingCost(heuristic, start, goal), std::nullopt, false}};
        // A slot per cell of the grid holding the index of its node, if it has one, so a
        // connection's destination is found without a scan.
        constexpr int NoNode = -1;
        const auto slotOf = [grid](GridPosition cell)
        {
            return static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(grid.width) +
                   static_cast<std::size_t>(cell.x);
        };
        std::vector<int> nodeAt(
            static_cast<std::size_t>(grid.width) * static_cast<std::size_t>(grid.height), NoNode);
        nodeAt[slotOf(start)] = 0;

        // Relaxes one connection leaving the cell being expanded: each connection may
        // lower the best known cost of its destination.
        const auto relax =
            [&](const NavigationConnection& connection, std::size_t parentIndex, int parentCost)
        {
            const int nextCost = parentCost + connection.cost;
            int& existing = nodeAt[slotOf(connection.step.destinationCell)];
            if (existing == NoNode)
            {
                existing = static_cast<int>(nodes.size());
                nodes.push_back(
                    {connection.step.destinationCell,
                     nextCost,
                     nextCost +
                         estimateRemainingCost(heuristic, connection.step.destinationCell, goal),
                     IncomingStep{parentIndex, connection.step},
                     false});
                return;
            }

            SearchNode& known = nodes[static_cast<std::size_t>(existing)];
            if (nextCost < known.costFromStart)
            {
                known.costFromStart = nextCost;
                known.estimatedTotalCost =
                    nextCost +
                    estimateRemainingCost(heuristic, connection.step.destinationCell, goal);
                known.incoming = IncomingStep{parentIndex, connection.step};
                known.closed = false;
            }
        };

        // The search: take the open node with the lowest estimated total, cost so far
        // plus the heuristic's guess of the rest. If it is the goal, the path is found.
        // Otherwise close it and relax its connections. A cheaper route can reopen a
        // closed node; a found path is returned when the goal is the cheapest open node.
        while (true)
        {
            const std::optional<std::size_t> currentIndex = cheapestOpenNode(nodes);
            if (!currentIndex.has_value())
            {
                // Nothing left to expand: every node is closed, and together they are
                // every cell the start leads to.
                PathSearchResult result;
                result.reachableCells.reserve(nodes.size());
                for (const SearchNode& node : nodes)
                {
                    result.reachableCells.push_back(node.cell);
                }
                return result;
            }

            SearchNode& currentNode = nodes[currentIndex.value()];
            if (currentNode.cell == goal)
            {
                return {
                    PathSearchStatus::Found,
                    reconstructPath(nodes, currentIndex.value()),
                    {},
                    std::nullopt};
            }

            const GridPosition currentCell = currentNode.cell;
            if (canExpand && !canExpand(currentCell))
            {
                return {PathSearchStatus::Incomplete, std::nullopt, {}, currentCell};
            }
            const std::size_t parentIndex = currentIndex.value();
            const int parentCost = currentNode.costFromStart;
            currentNode.closed = true;
            // Relaxing may add nodes, which can move them all, so currentNode is not used
            // after this. Connection order still determines ties between valid candidates.
            const std::vector<GridPosition> candidates = neighbors(currentCell);
            for (const GridPosition candidate : candidates)
            {
                if (!contains(grid, candidate))
                {
                    throw std::invalid_argument("A navigation neighbor lies outside the grid");
                }
            }
            for (const NavigationConnection& connection : connections(currentCell))
            {
                if (connection.cost <= 0)
                {
                    throw std::invalid_argument("A navigation connection must have positive cost");
                }
                if (!contains(grid, connection.step.destinationCell))
                {
                    throw std::invalid_argument("A connection leads outside the grid");
                }
                if (std::find(
                        candidates.begin(), candidates.end(), connection.step.destinationCell) !=
                    candidates.end())
                {
                    relax(connection, parentIndex, parentCost);
                }
            }
        }
    }
}
