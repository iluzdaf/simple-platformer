#include "simple_platformer/navigation/a_star.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"

namespace
{
    struct SearchNode
    {
        simple_platformer::GridPosition position;
        int costFromStart = 0;
        int estimatedTotalCost = 0;
        std::optional<simple_platformer::GridPosition> parent;
        bool closed = false;
    };

    int estimatedDistance(
        simple_platformer::GridPosition first,
        simple_platformer::GridPosition second)
    {
        return std::abs(first.x - second.x) + std::abs(first.y - second.y);
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

    std::vector<simple_platformer::GridPosition> reconstructPath(
        const std::vector<SearchNode>& nodes,
        simple_platformer::GridPosition goal)
    {
        std::vector<simple_platformer::GridPosition> path;
        simple_platformer::GridPosition current = goal;
        while (true)
        {
            path.push_back(current);
            const std::optional<std::size_t> node = findNode(nodes, current);
            if (!node.has_value() || !nodes[node.value_or(0)].parent.has_value())
            {
                break;
            }
            current = nodes[node.value_or(0)].parent.value_or(current);
        }
        std::reverse(path.begin(), path.end());
        return path;
    }
}

namespace simple_platformer
{
    std::optional<std::vector<GridPosition>> findGridPath(
        GridPosition start,
        GridPosition goal,
        const GridNeighborFunction& neighbors)
    {
        if (!neighbors)
        {
            throw std::invalid_argument("A* requires a neighbor function");
        }

        std::vector<SearchNode> nodes{
            {start, 0, estimatedDistance(start, goal), std::nullopt, false}};
        while (true)
        {
            const std::optional<std::size_t> currentIndex = cheapestOpenNode(nodes);
            if (!currentIndex.has_value())
            {
                return std::nullopt;
            }

            SearchNode& currentNode = nodes[currentIndex.value_or(0)];
            if (currentNode.position == goal)
            {
                return reconstructPath(nodes, goal);
            }

            const GridPosition currentPosition = currentNode.position;
            const int nextCost = currentNode.costFromStart + 1;
            currentNode.closed = true;

            for (const GridPosition neighbor : neighbors(currentPosition))
            {
                const std::optional<std::size_t> existingIndex = findNode(nodes, neighbor);
                if (!existingIndex.has_value())
                {
                    nodes.push_back(
                        {neighbor,
                         nextCost,
                         nextCost + estimatedDistance(neighbor, goal),
                         currentPosition,
                         false});
                    continue;
                }

                SearchNode& existing = nodes[existingIndex.value_or(0)];
                if (!existing.closed && nextCost < existing.costFromStart)
                {
                    existing.costFromStart = nextCost;
                    existing.estimatedTotalCost = nextCost + estimatedDistance(neighbor, goal);
                    existing.parent = currentPosition;
                }
            }
        }
    }
}
