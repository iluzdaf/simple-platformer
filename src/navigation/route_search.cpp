#include "simple_platformer/navigation/route_search.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/route.hpp"

namespace simple_platformer
{
    namespace
    {
        // How a node was reached in the cheapest way, for walking the route back to the start.
        struct IncomingStep
        {
            std::size_t parentIndex;
            RouteStep step;
        };

        // A location the search has reached. Closed once expanded, and opened again if a
        // cheaper way to it turns up.
        struct SearchNode
        {
            RouteLocation location;
            int costFromStart = 0;
            std::optional<IncomingStep> incoming;
            bool closed = false;
        };

        // Every location of a cell has its own node slot.
        constexpr int SurfacesPerCell = 4;

        int estimateRemainingCost(
            const HeuristicFunction& heuristic,
            RouteLocation location,
            Cell goal)
        {
            const int estimate = heuristic(location.cell, goal);
            if (estimate < 0)
            {
                throw std::invalid_argument("A route heuristic cannot return a negative cost");
            }
            return estimate;
        }

        // A node waiting to be expanded. It holds the node's index rather than a copy, so
        // the frontier stays cheap to reorder, and the index survives nodes growing.
        struct FrontierEntry
        {
            int estimatedTotalCost;
            std::size_t nodeIndex;
        };

        // The frontier's ordering. A std::priority_queue hands out its highest-priority
        // entry first, whatever order entries went in. This comparison makes the cheapest
        // estimate the highest priority. An entry that costs more comes out later.
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

        Route reconstructRoute(const std::vector<SearchNode>& nodes, std::size_t goalIndex)
        {
            std::vector<RouteStep> steps;
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

        std::size_t slotOf(GridSize grid, RouteLocation location)
        {
            const std::size_t cell =
                static_cast<std::size_t>(location.cell.y) * static_cast<std::size_t>(grid.width) +
                static_cast<std::size_t>(location.cell.x);
            return cell * static_cast<std::size_t>(SurfacesPerCell) +
                   static_cast<std::size_t>(location.surface);
        }

        bool containsLocation(GridSize grid, RouteLocation location)
        {
            const int surface = static_cast<int>(location.surface);
            return contains(grid, location.cell) && surface >= 0 && surface < SurfacesPerCell;
        }
    }

    RouteSearchResult findLowestCostRoute(
        RouteLocation start,
        Cell goal,
        GridSize grid,
        const ConnectionFunction& connections,
        const HeuristicFunction& heuristic,
        const ExpansionReady& canExpand)
    {
        if (!connections)
        {
            throw std::invalid_argument("Route search requires a connection function");
        }
        if (!heuristic)
        {
            throw std::invalid_argument("Route search requires a heuristic function");
        }
        if (grid.width <= 0 || grid.height <= 0)
        {
            throw std::invalid_argument("Route search requires a grid with cells");
        }
        if (!containsLocation(grid, start))
        {
            throw std::invalid_argument("Route search start must lie within the grid");
        }

        // Every location reached so far, in the order first reached; the start is node 0.
        std::vector<SearchNode> nodes{{start, 0, std::nullopt, false}};

        // Every location on the grid gets a slot in nodeAt, holding the index of its node
        // once the search reaches it, or NoNode until then.
        constexpr int NoNode = -1;
        std::vector<int> nodeAt(
            static_cast<std::size_t>(grid.width) * static_cast<std::size_t>(grid.height) *
                static_cast<std::size_t>(SurfacesPerCell),
            NoNode);
        nodeAt[slotOf(grid, start)] = 0;

        // The open nodes to expand, cheapest estimated total first.
        std::priority_queue<FrontierEntry, std::vector<FrontierEntry>, MoreExpensive> frontier;
        frontier.push({estimateRemainingCost(heuristic, start, goal), 0});

        // Checks whether going through the parent is the cheapest way found so far to the
        // place the connection leads. If it is, that place remembers the parent as its way
        // in and goes on the frontier to be expanded; if not, nothing changes.
        const auto relax =
            [&](const RouteConnection& connection, std::size_t parentIndex, int parentCost)
        {
            const RouteLocation destination = connection.step.destination;
            if (connection.cost <= 0)
            {
                throw std::invalid_argument("A route connection must have positive cost");
            }
            if (!containsLocation(grid, destination))
            {
                throw std::invalid_argument("A connection leads outside the grid");
            }
            if (parentCost > std::numeric_limits<int>::max() - connection.cost)
            {
                throw std::overflow_error("A route connection cost is too large");
            }

            const int nextCost = parentCost + connection.cost;
            int& existing = nodeAt[slotOf(grid, destination)];

            // 1. The place already has a way in that costs no more than going through the
            //    parent so nothing changes.
            if (existing != NoNode &&
                nextCost >= nodes[static_cast<std::size_t>(existing)].costFromStart)
            {
                return;
            }

            const int estimate = estimateRemainingCost(heuristic, destination, goal);
            if (nextCost > std::numeric_limits<int>::max() - estimate)
            {
                throw std::overflow_error("A route estimate is too large");
            }

            const int total = nextCost + estimate;

            // 2. The place is reached for the first time. Add its node, with the parent as
            //    its way in, and put it on the frontier.
            if (existing == NoNode)
            {
                existing = static_cast<int>(nodes.size());
                nodes.push_back(
                    {destination, nextCost, IncomingStep{parentIndex, connection.step}, false});
                frontier.push({total, static_cast<std::size_t>(existing)});
                return;
            }

            // 3. Going through the parent is cheaper than the place's way in so far: make the
            //    parent its way in, and put it back on the frontier. If it was already
            //    expanded this reopens it, so the saving reaches the places beyond it.
            SearchNode& known = nodes[static_cast<std::size_t>(existing)];
            known.costFromStart = nextCost;
            known.incoming = IncomingStep{parentIndex, connection.step};
            known.closed = false;
            frontier.push({total, static_cast<std::size_t>(existing)});
        };

        while (!frontier.empty())
        {
            // 1. Take the cheapest entry. Skip it if its node has already been expanded. That
            //    happens when a cheaper way to a node was found: relax pushes a new entry but
            //    doesn't remove the old one and the new one comes out first.
            const FrontierEntry next = frontier.top();
            frontier.pop();
            SearchNode& node = nodes[next.nodeIndex];
            if (node.closed)
            {
                continue;
            }

            // 2. The first location in the goal cell taken off the frontier is the
            //    cheapest one because the heuristic never overestimates.
            if (node.location.cell == goal)
            {
                return {reconstructRoute(nodes, next.nodeIndex), std::nullopt};
            }

            // 3. A location the caller is not ready to expand pauses the search there.
            const RouteLocation location = node.location;
            if (canExpand && !canExpand(location))
            {
                return {std::nullopt, location};
            }

            // 4. Expand it: close it, then for each place one step away, check whether going
            //    through this node is the cheapest way there found so far.
            const int parentCost = node.costFromStart;
            node.closed = true;
            for (const RouteConnection& connection : connections(location))
            {
                relax(connection, next.nodeIndex, parentCost);
            }
        }

        // The frontier ran out before the goal cell was reached. Return a route leading to
        // a cell as close to the goal as possible.
        RouteSearchResult result;
        std::size_t closest = 0;
        long long closestDistance = std::numeric_limits<long long>::max();
        for (std::size_t index = 0; index < nodes.size(); ++index)
        {
            const Cell cell = nodes[index].location.cell;
            const long long dx = static_cast<long long>(cell.x) - goal.x;
            const long long dy = static_cast<long long>(cell.y) - goal.y;
            const long long distanceSquared = dx * dx + dy * dy;
            if (distanceSquared < closestDistance)
            {
                closest = index;
                closestDistance = distanceSquared;
            }
        }
        result.route = reconstructRoute(nodes, closest);
        return result;
    }
}
