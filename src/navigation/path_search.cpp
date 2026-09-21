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
            GridPosition position;
            int costFromStart = 0;
            int estimatedTotalCost = 0;
            std::optional<IncomingConnection> incoming;
            bool closed = false;
        };

        int estimateRemainingCost(
            const GridHeuristicFunction& heuristic,
            GridPosition position,
            GridPosition goal)
        {
            const int estimate = heuristic(position, goal);
            if (estimate < 0)
            {
                throw std::invalid_argument("A path heuristic cannot return a negative cost");
            }
            return estimate;
        }

        std::optional<std::size_t> findNode(
            const std::vector<SearchNode>& nodes,
            GridPosition position)
        {
            const auto node = std::find_if(
                nodes.begin(),
                nodes.end(),
                [position](const SearchNode& candidate) { return candidate.position == position; });
            if (node == nodes.end())
            {
                return std::nullopt;
            }
            return static_cast<std::size_t>(node - nodes.begin());
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
                    {incoming.neighbor.destination,
                     incoming.neighbor.traversal,
                     incoming.neighbor.inputs});
                current = incoming.parentIndex;
            }
            std::reverse(steps.begin(), steps.end());
            return {nodes[current].position, std::move(steps)};
        }
    }

    int manhattanHeuristic(GridPosition position, GridPosition goal)
    {
        return std::abs(position.x - goal.x) + std::abs(position.y - goal.y);
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
        const GridHeuristicFunction& heuristic)
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
        while (true)
        {
            const std::optional<std::size_t> currentIndex = cheapestOpenNode(nodes);
            if (!currentIndex.has_value())
            {
                return std::nullopt;
            }

            SearchNode& currentNode = nodes[currentIndex.value()];
            if (currentNode.position == goal)
            {
                return reconstructPath(nodes, currentIndex.value());
            }

            const GridPosition currentPosition = currentNode.position;
            const int costFromStart = currentNode.costFromStart;
            currentNode.closed = true;

            for (const NavigationNeighbor& neighbor : neighbors(currentPosition))
            {
                if (neighbor.cost <= 0)
                {
                    throw std::invalid_argument("A navigation connection must have positive cost");
                }
                const int nextCost = costFromStart + neighbor.cost;
                const std::optional<std::size_t> existingIndex =
                    findNode(nodes, neighbor.destination);
                if (!existingIndex.has_value())
                {
                    nodes.push_back(
                        {neighbor.destination,
                         nextCost,
                         nextCost + estimateRemainingCost(heuristic, neighbor.destination, goal),
                         IncomingConnection{currentIndex.value(), neighbor},
                         false});
                    continue;
                }

                SearchNode& existing = nodes[existingIndex.value()];
                if (nextCost < existing.costFromStart)
                {
                    existing.costFromStart = nextCost;
                    existing.estimatedTotalCost =
                        nextCost + estimateRemainingCost(heuristic, neighbor.destination, goal);
                    existing.incoming = IncomingConnection{currentIndex.value(), neighbor};
                    existing.closed = false;
                }
            }
        }
    }
}
