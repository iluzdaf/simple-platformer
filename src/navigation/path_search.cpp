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

namespace
{
    struct SearchNode
    {
        simple_platformer::GridPosition position;
        int costFromStart = 0;
        int estimatedTotalCost = 0;
        std::optional<std::size_t> parent;
        std::optional<simple_platformer::NavigationNeighbor> connectionFromParent;
        bool closed = false;
    };

    int estimateRemainingCost(
        const simple_platformer::GridHeuristicFunction& heuristic,
        simple_platformer::GridPosition position,
        simple_platformer::GridPosition goal)
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
        simple_platformer::GridPosition position)
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
            if (!cheapest.has_value() ||
                nodes[index].estimatedTotalCost < nodes[cheapest.value_or(0)].estimatedTotalCost)
            {
                cheapest = index;
            }
        }
        return cheapest;
    }

    simple_platformer::NavigationPath reconstructPath(
        const std::vector<SearchNode>& nodes,
        std::size_t goalIndex)
    {
        std::vector<simple_platformer::NavigationStep> steps;
        std::size_t current = goalIndex;
        while (nodes[current].parent.has_value())
        {
            const SearchNode& currentNode = nodes[current];
            if (!currentNode.connectionFromParent.has_value())
            {
                throw std::logic_error("A path-search node is missing its incoming connection");
            }
            const simple_platformer::NavigationNeighbor connection =
                currentNode.connectionFromParent.value_or(simple_platformer::NavigationNeighbor{});
            steps.push_back({connection.destination, connection.traversal, connection.inputs});
            current = currentNode.parent.value_or(0);
        }
        std::reverse(steps.begin(), steps.end());
        return {nodes[current].position, std::move(steps)};
    }
}

namespace simple_platformer
{
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
            {start,
             0,
             estimateRemainingCost(heuristic, start, goal),
             std::nullopt,
             std::nullopt,
             false}};
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
                         currentIndex,
                         neighbor,
                         false});
                    continue;
                }

                SearchNode& existing = nodes[existingIndex.value()];
                if (nextCost < existing.costFromStart)
                {
                    existing.costFromStart = nextCost;
                    existing.estimatedTotalCost =
                        nextCost + estimateRemainingCost(heuristic, neighbor.destination, goal);
                    existing.parent = currentIndex;
                    existing.connectionFromParent = neighbor;
                    existing.closed = false;
                }
            }
        }
    }
}
