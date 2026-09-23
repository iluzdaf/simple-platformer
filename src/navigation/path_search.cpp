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
        struct IncomingConnection
        {
            std::size_t parentIndex;
            NavigationNeighbor neighbor;
        };

        struct SearchNode
        {
            GridPosition cell;
            int costFromStart = 0;
            int estimatedTotalCost = 0;
            std::optional<IncomingConnection> incoming;
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

                const IncomingConnection& incoming = currentNode.incoming.value();
                steps.push_back(
                    {incoming.neighbor.destinationCell,
                     incoming.neighbor.traversal,
                     incoming.neighbor.inputs});
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

    std::optional<NavigationPath> findLowestCostPath(
        GridPosition start,
        GridPosition goal,
        GridSize grid,
        const GridNeighborFunction& neighbors)
    {
        const GridHeuristicFunction noHeuristic = [](GridPosition, GridPosition) { return 0; };
        return findLowestCostPath(start, goal, grid, neighbors, noHeuristic);
    }

    std::optional<NavigationPath> findLowestCostPath(
        GridPosition start,
        GridPosition goal,
        GridSize grid,
        const GridNeighborFunction& neighbors,
        const GridHeuristicFunction& heuristic,
        PathSearchStatistics* statistics,
        std::vector<GridPosition>* reached)
    {
        if (!neighbors)
        {
            throw std::invalid_argument("Path search requires a neighbor function");
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

        // Relaxes one connection leaving the cell being expanded. Built once: a callback
        // built for every cell would be allocated for every cell.
        std::size_t expandedIndex = 0;
        int costFromStart = 0;
        const GridNeighborVisitor relax = [&](const NavigationNeighbor& neighbor, int cost)
        {
            if (cost <= 0)
            {
                throw std::invalid_argument("A navigation connection must have positive cost");
            }
            if (!contains(grid, neighbor.destinationCell))
            {
                throw std::invalid_argument("A connection leads outside the grid");
            }
            const int nextCost = costFromStart + cost;
            int& existing = nodeAt[slotOf(neighbor.destinationCell)];
            if (existing == NoNode)
            {
                existing = static_cast<int>(nodes.size());
                nodes.push_back(
                    {neighbor.destinationCell,
                     nextCost,
                     nextCost + estimateRemainingCost(heuristic, neighbor.destinationCell, goal),
                     IncomingConnection{expandedIndex, neighbor},
                     false});
                return;
            }

            SearchNode& known = nodes[static_cast<std::size_t>(existing)];
            if (nextCost < known.costFromStart)
            {
                known.costFromStart = nextCost;
                known.estimatedTotalCost =
                    nextCost + estimateRemainingCost(heuristic, neighbor.destinationCell, goal);
                known.incoming = IncomingConnection{expandedIndex, neighbor};
                known.closed = false;
            }
        };

        while (true)
        {
            const std::optional<std::size_t> currentIndex = cheapestOpenNode(nodes);
            if (!currentIndex.has_value())
            {
                // Nothing left to expand: every node is closed, and together they are
                // every cell the start leads to.
                if (reached != nullptr)
                {
                    reached->clear();
                    reached->reserve(nodes.size());
                    for (const SearchNode& node : nodes)
                    {
                        reached->push_back(node.cell);
                    }
                }
                return std::nullopt;
            }

            SearchNode& currentNode = nodes[currentIndex.value()];
            if (currentNode.cell == goal)
            {
                return reconstructPath(nodes, currentIndex.value());
            }

            const GridPosition currentCell = currentNode.cell;
            expandedIndex = currentIndex.value();
            costFromStart = currentNode.costFromStart;
            currentNode.closed = true;
            if (statistics != nullptr)
            {
                ++statistics->nodesExpanded;
            }

            // Adding a node may move the others, so nothing above is referenced below.
            neighbors(currentCell, relax);
        }
    }
}
