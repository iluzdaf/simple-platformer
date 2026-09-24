#include "simple_platformer/navigation/connection_cache.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    bool operator==(const ConnectionBody& left, const ConnectionBody& right)
    {
        return left.size == right.size && left.movement == right.movement &&
               left.stepSeconds == right.stepSeconds;
    }

    bool operator==(const PathQuery& left, const PathQuery& right)
    {
        return left.start == right.start && left.goal == right.goal &&
               left.jumpStartPenaltyTicks == right.jumpStartPenaltyTicks;
    }

    std::size_t PathQueryHash::operator()(const PathQuery& query) const
    {
        // Each part mixed into the seed the way Boost's hash_combine does.
        const GridPositionHash cell;
        std::size_t seed = cell(query.start);
        seed ^= cell(query.goal) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        seed ^= static_cast<std::size_t>(query.jumpStartPenaltyTicks) + 0x9e3779b9U + (seed << 6U) +
                (seed >> 2U);
        return seed;
    }

    void PlatformerConnectionCache::requireValid(const ConnectionBody& body) const
    {
        if (!isFinite(body.size) || body.size.x <= 0.0F || body.size.y <= 0.0F ||
            !isFinitePositive(body.stepSeconds))
        {
            throw std::invalid_argument(
                "Connections are kept for a finite, positive body size and step");
        }
    }

    const PlatformerConnectionCache::BodyConnections* PlatformerConnectionCache::findConnectionsFor(
        const ConnectionBody& body) const
    {
        for (const BodyConnections& kept : bodies)
        {
            if (kept.body == body)
            {
                return &kept;
            }
        }
        return nullptr;
    }

    PlatformerConnectionCache::BodyConnections* PlatformerConnectionCache::findConnectionsFor(
        const ConnectionBody& body)
    {
        for (BodyConnections& kept : bodies)
        {
            if (kept.body == body)
            {
                return &kept;
            }
        }
        return nullptr;
    }

    PlatformerConnectionCache::BodyConnections& PlatformerConnectionCache::connectionsFor(
        const ConnectionBody& body)
    {
        for (BodyConnections& kept : bodies)
        {
            if (kept.body == body)
            {
                return kept;
            }
        }
        BodyConnections fresh;
        fresh.body = body;
        bodies.push_back(std::move(fresh));
        return bodies.back();
    }

    void PlatformerConnectionCache::syncWith(const TileMap& map)
    {
        const std::vector<GridPosition>& broken = map.brokenCells();
        for (; breaksSeen < broken.size(); ++breaksSeen)
        {
            invalidate(broken[breaksSeen]);
        }
    }

    void PlatformerConnectionCache::invalidate(GridPosition brokenCell)
    {
        for (BodyConnections& kept : bodies)
        {
            std::vector<GridPosition> dropped;
            for (auto entry = kept.cells.begin(); entry != kept.cells.end();)
            {
                if (contains(entry->second.footprint, brokenCell))
                {
                    dropped.push_back(entry->first);
                    kept.pending.push_back(entry->first);
                    kept.waiting.insert(entry->first);
                    ++dropsSoFar;
                    entry = kept.cells.erase(entry);
                }
                else
                {
                    ++entry;
                }
            }
            // A reachable set is the closure of its cells' connections, so it holds only
            // while none of its cells has changed.
            for (auto set = kept.reachable.begin(); set != kept.reachable.end();)
            {
                const std::vector<GridPosition>& cells = set->second;
                const bool touched = std::any_of(
                    dropped.begin(),
                    dropped.end(),
                    [&cells](GridPosition cell)
                    { return std::find(cells.begin(), cells.end(), cell) != cells.end(); });
                set = touched ? kept.reachable.erase(set) : std::next(set);
            }
            kept.paths.clear();
        }
    }

    const std::vector<NavigationNeighbor>* PlatformerConnectionCache::find(
        GridPosition cell,
        const ConnectionBody& body) const
    {
        const BodyConnections* kept = findConnectionsFor(body);
        if (kept == nullptr)
        {
            return nullptr;
        }
        const auto connections = kept->cells.find(cell);
        return connections == kept->cells.end() ? nullptr : &connections->second.connections;
    }

    std::optional<CellRange> PlatformerConnectionCache::footprintKept(
        GridPosition cell,
        const ConnectionBody& body) const
    {
        const BodyConnections* kept = findConnectionsFor(body);
        if (kept == nullptr)
        {
            return std::nullopt;
        }
        const auto connections = kept->cells.find(cell);
        if (connections == kept->cells.end())
        {
            return std::nullopt;
        }
        return connections->second.footprint;
    }

    const std::vector<NavigationNeighbor>& PlatformerConnectionCache::keep(
        GridPosition cell,
        const ConnectionBody& body,
        std::vector<NavigationNeighbor> connections,
        const CellRange& footprint)
    {
        requireValid(body);
        BodyConnections& forBody = connectionsFor(body);
        if (forBody.waiting.erase(cell) > 0)
        {
            forBody.pending.erase(std::find(forBody.pending.begin(), forBody.pending.end(), cell));
        }
        KeptConnections& kept = forBody.cells[cell];
        kept = {std::move(connections), footprint};
        ++keepsSoFar;
        return kept.connections;
    }

    const RememberedWalk* PlatformerConnectionCache::walkKept(
        int columns,
        const ConnectionBody& body) const
    {
        const BodyConnections* kept = findConnectionsFor(body);
        if (kept == nullptr)
        {
            return nullptr;
        }
        const auto walk = kept->walks.find(columns);
        return walk == kept->walks.end() ? nullptr : &walk->second;
    }

    void PlatformerConnectionCache::keepWalk(
        int columns,
        const ConnectionBody& body,
        const RememberedWalk& walk)
    {
        requireValid(body);
        connectionsFor(body).walks[columns] = walk;
    }

    const std::vector<GridPosition>* PlatformerConnectionCache::reachableFrom(
        GridPosition start,
        const ConnectionBody& body) const
    {
        const BodyConnections* kept = findConnectionsFor(body);
        if (kept == nullptr)
        {
            return nullptr;
        }
        const auto cells = kept->reachable.find(start);
        return cells == kept->reachable.end() ? nullptr : &cells->second;
    }

    void PlatformerConnectionCache::keepReachable(
        GridPosition start,
        const ConnectionBody& body,
        std::vector<GridPosition> cells)
    {
        requireValid(body);
        connectionsFor(body).reachable[start] = std::move(cells);
    }

    const NavigationPath* PlatformerConnectionCache::pathKept(
        const PathQuery& query,
        const ConnectionBody& body) const
    {
        const BodyConnections* kept = findConnectionsFor(body);
        if (kept == nullptr)
        {
            return nullptr;
        }
        const auto path = kept->paths.find(query);
        return path == kept->paths.end() ? nullptr : &path->second;
    }

    void PlatformerConnectionCache::keepPath(
        const PathQuery& query,
        const ConnectionBody& body,
        NavigationPath path)
    {
        requireValid(body);
        connectionsFor(body).paths[query] = std::move(path);
    }

    void PlatformerConnectionCache::clear()
    {
        bodies.clear();
        breaksSeen = 0;
        dropsSoFar = 0;
        keepsSoFar = 0;
    }

    void PlatformerConnectionCache::queue(GridPosition cell, const ConnectionBody& body)
    {
        requireValid(body);
        BodyConnections& forBody = connectionsFor(body);
        if (forBody.cells.count(cell) > 0 || forBody.waiting.count(cell) > 0)
        {
            return;
        }
        forBody.pending.push_back(cell);
        forBody.waiting.insert(cell);
    }

    std::size_t PlatformerConnectionCache::cellsPending(const ConnectionBody& body) const
    {
        const BodyConnections* kept = findConnectionsFor(body);
        return kept == nullptr ? 0 : kept->pending.size();
    }

    bool PlatformerConnectionCache::isPending(GridPosition cell, const ConnectionBody& body) const
    {
        const BodyConnections* kept = findConnectionsFor(body);
        return kept != nullptr && kept->waiting.count(cell) > 0;
    }

    std::optional<GridPosition> PlatformerConnectionCache::nextPending(
        const ConnectionBody& body) const
    {
        const BodyConnections* kept = findConnectionsFor(body);
        if (kept == nullptr || kept->pending.empty())
        {
            return std::nullopt;
        }
        return kept->pending.front();
    }

    void PlatformerConnectionCache::prioritise(GridPosition cell, const ConnectionBody& body)
    {
        BodyConnections* kept = findConnectionsFor(body);
        if (kept == nullptr)
        {
            return;
        }
        if (kept->waiting.count(cell) == 0 || kept->pending.front() == cell)
        {
            return;
        }
        kept->pending.erase(std::find(kept->pending.begin(), kept->pending.end(), cell));
        kept->pending.push_front(cell);
    }

    std::size_t PlatformerConnectionCache::cellsKept(const ConnectionBody& body) const
    {
        const BodyConnections* kept = findConnectionsFor(body);
        return kept == nullptr ? 0 : kept->cells.size();
    }

    std::size_t PlatformerConnectionCache::cellsConnected(const ConnectionBody& body) const
    {
        const BodyConnections* kept = findConnectionsFor(body);
        if (kept == nullptr)
        {
            return 0;
        }
        return static_cast<std::size_t>(std::count_if(
            kept->cells.begin(),
            kept->cells.end(),
            [](const auto& entry) { return !entry.second.connections.empty(); }));
    }

    std::size_t PlatformerConnectionCache::walksKept(const ConnectionBody& body) const
    {
        const BodyConnections* kept = findConnectionsFor(body);
        return kept == nullptr ? 0 : kept->walks.size();
    }

    std::size_t PlatformerConnectionCache::reachableSetsKept(const ConnectionBody& body) const
    {
        const BodyConnections* kept = findConnectionsFor(body);
        return kept == nullptr ? 0 : kept->reachable.size();
    }

    std::size_t PlatformerConnectionCache::pathsKept(const ConnectionBody& body) const
    {
        const BodyConnections* kept = findConnectionsFor(body);
        return kept == nullptr ? 0 : kept->paths.size();
    }

    std::size_t PlatformerConnectionCache::breaksApplied() const
    {
        return breaksSeen;
    }

    std::size_t PlatformerConnectionCache::cellsDroppedSoFar() const
    {
        return dropsSoFar;
    }

    std::size_t PlatformerConnectionCache::cellsKeptSoFar() const
    {
        return keepsSoFar;
    }

    std::vector<ConnectionBody> PlatformerConnectionCache::bodiesKept() const
    {
        std::vector<ConnectionBody> known;
        known.reserve(bodies.size());
        for (const BodyConnections& kept : bodies)
        {
            known.push_back(kept.body);
        }
        return known;
    }

    std::size_t PlatformerConnectionCache::size() const
    {
        std::size_t total = 0;
        for (const BodyConnections& kept : bodies)
        {
            total += kept.cells.size();
        }
        return total;
    }
}
