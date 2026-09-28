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
#include "simple_platformer/navigation/platformer_connections.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    bool operator==(const PathQuery& left, const PathQuery& right)
    {
        return left.start == right.start && left.goal == right.goal &&
               left.jumpStartPenaltyTicks == right.jumpStartPenaltyTicks;
    }

    std::size_t PathQueryHash::operator()(const PathQuery& query) const
    {
        const GridPositionHash cell;
        std::size_t seed = cell(query.start);
        seed ^= cell(query.goal) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        seed ^= static_cast<std::size_t>(query.jumpStartPenaltyTicks) + 0x9e3779b9U + (seed << 6U) +
                (seed >> 2U);
        return seed;
    }

    void PlatformerConnectionCache::requireValid(const PlatformerTraversalProfile& profile) const
    {
        if (!isFinite(profile.size) || profile.size.x <= 0.0F || profile.size.y <= 0.0F ||
            !isFinitePositive(profile.stepSeconds))
        {
            throw std::invalid_argument(
                "Connections require a finite, positive profile size and step");
        }
    }

    const PlatformerConnectionCache::ProfileCache* PlatformerConnectionCache::findCacheFor(
        const PlatformerTraversalProfile& profile) const
    {
        for (const ProfileCache& profileCache : profileCaches)
        {
            if (profileCache.profile == profile)
            {
                return &profileCache;
            }
        }
        return nullptr;
    }

    PlatformerConnectionCache::ProfileCache* PlatformerConnectionCache::findCacheFor(
        const PlatformerTraversalProfile& profile)
    {
        for (ProfileCache& profileCache : profileCaches)
        {
            if (profileCache.profile == profile)
            {
                return &profileCache;
            }
        }
        return nullptr;
    }

    PlatformerConnectionCache::ProfileCache& PlatformerConnectionCache::cacheFor(
        const PlatformerTraversalProfile& profile)
    {
        if (ProfileCache* profileCache = findCacheFor(profile))
        {
            return *profileCache;
        }
        ProfileCache fresh;
        fresh.profile = profile;
        profileCaches.push_back(std::move(fresh));
        return profileCaches.back();
    }

    void PlatformerConnectionCache::applyRecordedTileBreaks(const TileMap& map)
    {
        const std::vector<GridPosition>& broken = map.brokenCells();
        for (; breaksSeen < broken.size(); ++breaksSeen)
        {
            invalidate(broken[breaksSeen]);
        }
    }

    void PlatformerConnectionCache::invalidate(GridPosition brokenCell)
    {
        for (ProfileCache& profileCache : profileCaches)
        {
            std::vector<GridPosition> dropped;
            for (auto entry = profileCache.cells.begin(); entry != profileCache.cells.end();)
            {
                if (contains(entry->second.footprint, brokenCell))
                {
                    dropped.push_back(entry->first);
                    profileCache.pending.push_back(entry->first);
                    profileCache.waiting.insert(entry->first);
                    ++dropsSoFar;
                    entry = profileCache.cells.erase(entry);
                }
                else
                {
                    ++entry;
                }
            }
            // A reachable set is the closure of its cells' connections, so it holds only
            // while none of its cells has changed.
            for (auto set = profileCache.reachable.begin(); set != profileCache.reachable.end();)
            {
                const std::vector<GridPosition>& cells = set->second;
                const bool touched = std::any_of(
                    dropped.begin(),
                    dropped.end(),
                    [&cells](GridPosition cell)
                    { return std::find(cells.begin(), cells.end(), cell) != cells.end(); });
                set = touched ? profileCache.reachable.erase(set) : std::next(set);
            }
            profileCache.paths.clear();
        }
    }

    const std::vector<NavigationConnection>* PlatformerConnectionCache::cachedConnections(
        GridPosition cell,
        const PlatformerTraversalProfile& profile) const
    {
        const ProfileCache* profileCache = findCacheFor(profile);
        if (profileCache == nullptr)
        {
            return nullptr;
        }
        const auto connections = profileCache->cells.find(cell);
        return connections == profileCache->cells.end() ? nullptr
                                                        : &connections->second.connections;
    }

    std::optional<CellRange> PlatformerConnectionCache::cachedFootprint(
        GridPosition cell,
        const PlatformerTraversalProfile& profile) const
    {
        const ProfileCache* profileCache = findCacheFor(profile);
        if (profileCache == nullptr)
        {
            return std::nullopt;
        }
        const auto connections = profileCache->cells.find(cell);
        if (connections == profileCache->cells.end())
        {
            return std::nullopt;
        }
        return connections->second.footprint;
    }

    const std::vector<NavigationConnection>& PlatformerConnectionCache::storeConnections(
        GridPosition cell,
        const PlatformerTraversalProfile& profile,
        std::vector<NavigationConnection> connections,
        const CellRange& footprint)
    {
        requireValid(profile);
        ProfileCache& forProfile = cacheFor(profile);
        if (forProfile.waiting.erase(cell) > 0)
        {
            forProfile.pending.erase(
                std::find(forProfile.pending.begin(), forProfile.pending.end(), cell));
        }
        CachedConnections& cached = forProfile.cells[cell];
        cached = {std::move(connections), footprint};
        ++connectionWriteCount;
        return cached.connections;
    }

    const WalkSimulationResult* PlatformerConnectionCache::cachedWalk(
        int columns,
        const PlatformerTraversalProfile& profile) const
    {
        const ProfileCache* profileCache = findCacheFor(profile);
        if (profileCache == nullptr)
        {
            return nullptr;
        }
        const auto walk = profileCache->walks.find(columns);
        return walk == profileCache->walks.end() ? nullptr : &walk->second;
    }

    void PlatformerConnectionCache::storeWalk(
        const PlatformerTraversalProfile& profile,
        const WalkSimulationResult& walk)
    {
        requireValid(profile);
        cacheFor(profile).walks[walk.columns] = walk;
    }

    const std::vector<GridPosition>* PlatformerConnectionCache::cachedReachableCells(
        GridPosition start,
        const PlatformerTraversalProfile& profile) const
    {
        const ProfileCache* profileCache = findCacheFor(profile);
        if (profileCache == nullptr)
        {
            return nullptr;
        }
        const auto cells = profileCache->reachable.find(start);
        return cells == profileCache->reachable.end() ? nullptr : &cells->second;
    }

    void PlatformerConnectionCache::storeReachableCells(
        GridPosition start,
        const PlatformerTraversalProfile& profile,
        std::vector<GridPosition> cells)
    {
        requireValid(profile);
        cacheFor(profile).reachable[start] = std::move(cells);
    }

    const NavigationPath* PlatformerConnectionCache::cachedPath(
        const PathQuery& query,
        const PlatformerTraversalProfile& profile) const
    {
        const ProfileCache* profileCache = findCacheFor(profile);
        if (profileCache == nullptr)
        {
            return nullptr;
        }
        const auto path = profileCache->paths.find(query);
        return path == profileCache->paths.end() ? nullptr : &path->second;
    }

    void PlatformerConnectionCache::storePath(
        const PathQuery& query,
        const PlatformerTraversalProfile& profile,
        NavigationPath path)
    {
        requireValid(profile);
        cacheFor(profile).paths[query] = std::move(path);
    }

    void PlatformerConnectionCache::clear()
    {
        profileCaches.clear();
        breaksSeen = 0;
        dropsSoFar = 0;
        connectionWriteCount = 0;
    }

    void PlatformerConnectionCache::queue(
        GridPosition cell,
        const PlatformerTraversalProfile& profile)
    {
        requireValid(profile);
        ProfileCache& forProfile = cacheFor(profile);
        if (forProfile.cells.count(cell) > 0 || forProfile.waiting.count(cell) > 0)
        {
            return;
        }
        forProfile.pending.push_back(cell);
        forProfile.waiting.insert(cell);
    }

    std::size_t PlatformerConnectionCache::cellsPending(
        const PlatformerTraversalProfile& profile) const
    {
        const ProfileCache* profileCache = findCacheFor(profile);
        return profileCache == nullptr ? 0 : profileCache->pending.size();
    }

    bool PlatformerConnectionCache::isPending(
        GridPosition cell,
        const PlatformerTraversalProfile& profile) const
    {
        const ProfileCache* profileCache = findCacheFor(profile);
        return profileCache != nullptr && profileCache->waiting.count(cell) > 0;
    }

    std::optional<GridPosition> PlatformerConnectionCache::nextPending(
        const PlatformerTraversalProfile& profile) const
    {
        const ProfileCache* profileCache = findCacheFor(profile);
        if (profileCache == nullptr || profileCache->pending.empty())
        {
            return std::nullopt;
        }
        return profileCache->pending.front();
    }

    void PlatformerConnectionCache::prioritise(
        GridPosition cell,
        const PlatformerTraversalProfile& profile)
    {
        ProfileCache* profileCache = findCacheFor(profile);
        if (profileCache == nullptr)
        {
            return;
        }
        if (profileCache->waiting.count(cell) == 0 || profileCache->pending.front() == cell)
        {
            return;
        }
        profileCache->pending.erase(
            std::find(profileCache->pending.begin(), profileCache->pending.end(), cell));
        profileCache->pending.push_front(cell);
    }

    std::size_t PlatformerConnectionCache::cachedCellCount(
        const PlatformerTraversalProfile& profile) const
    {
        const ProfileCache* profileCache = findCacheFor(profile);
        return profileCache == nullptr ? 0 : profileCache->cells.size();
    }

    std::size_t PlatformerConnectionCache::cellsConnected(
        const PlatformerTraversalProfile& profile) const
    {
        const ProfileCache* profileCache = findCacheFor(profile);
        if (profileCache == nullptr)
        {
            return 0;
        }
        return static_cast<std::size_t>(std::count_if(
            profileCache->cells.begin(),
            profileCache->cells.end(),
            [](const auto& entry) { return !entry.second.connections.empty(); }));
    }

    std::size_t PlatformerConnectionCache::cachedWalkCount(
        const PlatformerTraversalProfile& profile) const
    {
        const ProfileCache* profileCache = findCacheFor(profile);
        return profileCache == nullptr ? 0 : profileCache->walks.size();
    }

    std::size_t PlatformerConnectionCache::cachedReachableSetCount(
        const PlatformerTraversalProfile& profile) const
    {
        const ProfileCache* profileCache = findCacheFor(profile);
        return profileCache == nullptr ? 0 : profileCache->reachable.size();
    }

    std::size_t PlatformerConnectionCache::cachedPathCount(
        const PlatformerTraversalProfile& profile) const
    {
        const ProfileCache* profileCache = findCacheFor(profile);
        return profileCache == nullptr ? 0 : profileCache->paths.size();
    }

    std::size_t PlatformerConnectionCache::breaksApplied() const
    {
        return breaksSeen;
    }

    std::size_t PlatformerConnectionCache::cellsDroppedSoFar() const
    {
        return dropsSoFar;
    }

    std::size_t PlatformerConnectionCache::connectionWritesSoFar() const
    {
        return connectionWriteCount;
    }

    std::vector<PlatformerTraversalProfile> PlatformerConnectionCache::knownProfiles() const
    {
        std::vector<PlatformerTraversalProfile> known;
        known.reserve(profileCaches.size());
        for (const ProfileCache& profileCache : profileCaches)
        {
            known.push_back(profileCache.profile);
        }
        return known;
    }

    std::size_t PlatformerConnectionCache::size() const
    {
        std::size_t total = 0;
        for (const ProfileCache& profileCache : profileCaches)
        {
            total += profileCache.cells.size();
        }
        return total;
    }
}
