#include "simple_platformer/navigation/path_search.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_graph.hpp"

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

        // A location the search has reached. Closed once expanded, and opened again if a
        // cheaper way to it turns up.
        struct SearchNode
        {
            NavigationLocation location;
            int costFromStart = 0;
            int estimatedTotalCost = 0;
            std::optional<IncomingStep> incoming;
            bool closed = false;
        };

        // Every location of a cell has its own node slot.
        constexpr int SurfacesPerCell = 4;

        int estimateRemainingCost(
            const HeuristicFunction& heuristic,
            NavigationLocation location,
            GridPosition goal)
        {
            const int estimate = heuristic(location.cell, goal);
            if (estimate < 0)
            {
                throw std::invalid_argument("A path heuristic cannot return a negative cost");
            }
            return estimate;
        }

        struct FrontierEntry
        {
            int estimatedTotalCost;
            std::size_t nodeIndex;
        };

        struct MoreExpensive
        {
            bool operator()(const FrontierEntry& left, const FrontierEntry& right) const
            {
                // Keep discovery order for equal estimates, including after a node reopens.
                return left.estimatedTotalCost == right.estimatedTotalCost
                           ? left.nodeIndex > right.nodeIndex
                           : left.estimatedTotalCost > right.estimatedTotalCost;
            }
        };

        LocationPath reconstructPath(const std::vector<SearchNode>& nodes, std::size_t goalIndex)
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
            return {nodes[current].location, std::move(steps)};
        }

        std::size_t slotOf(GridSize grid, NavigationLocation location)
        {
            const std::size_t cell =
                static_cast<std::size_t>(location.cell.y) * static_cast<std::size_t>(grid.width) +
                static_cast<std::size_t>(location.cell.x);
            return cell * static_cast<std::size_t>(SurfacesPerCell) +
                   static_cast<std::size_t>(location.surface);
        }

        bool containsLocation(GridSize grid, NavigationLocation location)
        {
            const int surface = static_cast<int>(location.surface);
            return contains(grid, location.cell) && surface >= 0 && surface < SurfacesPerCell;
        }
    }

    PathSearchResult findLowestCostPath(
        NavigationLocation start,
        GridPosition goal,
        GridSize grid,
        const ConnectionFunction& connections,
        const HeuristicFunction& heuristic,
        const ExpansionReady& canExpand)
    {
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
        if (!containsLocation(grid, start))
        {
            throw std::invalid_argument("Path search start must lie within the grid");
        }

        std::vector<SearchNode> nodes{
            {start, 0, estimateRemainingCost(heuristic, start, goal), std::nullopt, false}};
        constexpr int NoNode = -1;
        std::vector<int> nodeAt(
            static_cast<std::size_t>(grid.width) * static_cast<std::size_t>(grid.height) *
                static_cast<std::size_t>(SurfacesPerCell),
            NoNode);
        nodeAt[slotOf(grid, start)] = 0;
        std::priority_queue<FrontierEntry, std::vector<FrontierEntry>, MoreExpensive> frontier;
        frontier.push({nodes.front().estimatedTotalCost, 0});

        const auto relax =
            [&](const NavigationConnection& connection, std::size_t parentIndex, int parentCost)
        {
            const NavigationLocation destination{
                connection.step.destinationCell, connection.step.destinationSurface};
            if (connection.cost <= 0)
            {
                throw std::invalid_argument("A navigation connection must have positive cost");
            }
            if (!containsLocation(grid, destination))
            {
                throw std::invalid_argument("A connection leads outside the grid");
            }
            if (parentCost > std::numeric_limits<int>::max() - connection.cost)
            {
                throw std::overflow_error("A navigation connection cost is too large");
            }
            const int nextCost = parentCost + connection.cost;
            int& existing = nodeAt[slotOf(grid, destination)];
            if (existing != NoNode &&
                nextCost >= nodes[static_cast<std::size_t>(existing)].costFromStart)
            {
                return;
            }
            const int estimate = estimateRemainingCost(heuristic, destination, goal);
            if (nextCost > std::numeric_limits<int>::max() - estimate)
            {
                throw std::overflow_error("A navigation path estimate is too large");
            }
            const int total = nextCost + estimate;
            if (existing == NoNode)
            {
                existing = static_cast<int>(nodes.size());
                nodes.push_back(
                    {destination,
                     nextCost,
                     total,
                     IncomingStep{parentIndex, connection.step},
                     false});
                frontier.push({total, static_cast<std::size_t>(existing)});
                return;
            }

            SearchNode& known = nodes[static_cast<std::size_t>(existing)];
            known.costFromStart = nextCost;
            known.estimatedTotalCost = total;
            known.incoming = IncomingStep{parentIndex, connection.step};
            known.closed = false;
            frontier.push({total, static_cast<std::size_t>(existing)});
        };

        while (!frontier.empty())
        {
            const FrontierEntry next = frontier.top();
            frontier.pop();
            SearchNode& node = nodes[next.nodeIndex];
            if (node.closed || next.estimatedTotalCost != node.estimatedTotalCost)
            {
                continue;
            }
            if (node.location.cell == goal)
            {
                return {reconstructPath(nodes, next.nodeIndex), std::nullopt};
            }
            const NavigationLocation location = node.location;
            if (canExpand && !canExpand(location))
            {
                return {std::nullopt, location};
            }
            const int parentCost = node.costFromStart;
            node.closed = true;
            for (const NavigationConnection& connection : connections(location))
            {
                relax(connection, next.nodeIndex, parentCost);
            }
        }

        PathSearchResult result;
        std::size_t closest = 0;
        long long closestDistance = std::numeric_limits<long long>::max();
        for (std::size_t index = 0; index < nodes.size(); ++index)
        {
            const GridPosition cell = nodes[index].location.cell;
            const long long dx = static_cast<long long>(cell.x) - goal.x;
            const long long dy = static_cast<long long>(cell.y) - goal.y;
            const long long distanceSquared = dx * dx + dy * dy;
            if (distanceSquared < closestDistance)
            {
                closest = index;
                closestDistance = distanceSquared;
            }
        }
        result.path = reconstructPath(nodes, closest);
        return result;
    }
}
