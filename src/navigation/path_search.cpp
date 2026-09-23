#include "simple_platformer/navigation/path_search.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <unordered_map>
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
        const GridNeighborFunction& neighbors)
    {
        const GridHeuristicFunction noHeuristic = [](GridPosition, GridPosition) { return 0; };
        return findLowestCostPath(start, goal, neighbors, noHeuristic);
    }

    std::optional<NavigationPath> findLowestCostPath(
        GridPosition start,
        GridPosition goal,
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

        std::vector<SearchNode> nodes{
            {start, 0, estimateRemainingCost(heuristic, start, goal), std::nullopt, false}};
        // Where each cell's node is, so a connection's destination is found without a scan.
        std::unordered_map<GridPosition, std::size_t, GridPositionHash> nodeIndex{{start, 0}};
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
            const int costFromStart = currentNode.costFromStart;
            currentNode.closed = true;
            if (statistics != nullptr)
            {
                ++statistics->nodesExpanded;
            }

            // Adding a node may move the others, so nothing above is referenced below.
            neighbors(
                currentCell,
                [&](const NavigationNeighbor& neighbor, int cost)
                {
                    if (cost <= 0)
                    {
                        throw std::invalid_argument(
                            "A navigation connection must have positive cost");
                    }
                    const int nextCost = costFromStart + cost;
                    const auto existing = nodeIndex.find(neighbor.destinationCell);
                    if (existing == nodeIndex.end())
                    {
                        nodeIndex.emplace(neighbor.destinationCell, nodes.size());
                        nodes.push_back(
                            {neighbor.destinationCell,
                             nextCost,
                             nextCost +
                                 estimateRemainingCost(heuristic, neighbor.destinationCell, goal),
                             IncomingConnection{currentIndex.value(), neighbor},
                             false});
                        return;
                    }

                    SearchNode& known = nodes[existing->second];
                    if (nextCost < known.costFromStart)
                    {
                        known.costFromStart = nextCost;
                        known.estimatedTotalCost =
                            nextCost +
                            estimateRemainingCost(heuristic, neighbor.destinationCell, goal);
                        known.incoming = IncomingConnection{currentIndex.value(), neighbor};
                        known.closed = false;
                    }
                });
        }
    }
}
