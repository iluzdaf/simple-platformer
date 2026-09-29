#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_graph.hpp"
#include "simple_platformer/navigation/platformer_connections.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"

namespace simple_platformer
{
    class TileMap;

    // Stores simulated connections and reusable walk results for one map, separated by
    // traversal profile. A tile break invalidates connections whose footprints include
    // it; flat-floor walk results remain reusable. Writes require finite, positive size
    // and step values in the profile.
    class PlatformerConnectionCache
    {
    public:
        // Applies map breaks recorded since the last call. Searches and fill call this
        // before using cached connections.
        void applyRecordedTileBreaks(const TileMap& map);
        // Drops what one broken cell can have changed; recorded breaks use this rule.
        // Every cell dropped joins its profile's fill queue.
        void invalidate(GridPosition brokenCell);

        // The cells waiting to be cached, in the order the fill takes them: every cell of
        // the map when a level starts, and the cells a break drops after. Storing a cell
        // takes it off. A search that needs one before its turn moves it to the front.
        // Queuing a cell that is cached or waiting already changes nothing.
        void queue(GridPosition cell, const PlatformerTraversalProfile& profile);
        std::size_t cellsPending(const PlatformerTraversalProfile& profile) const;
        bool isPending(GridPosition cell, const PlatformerTraversalProfile& profile) const;
        std::optional<GridPosition> nextPending(const PlatformerTraversalProfile& profile) const;
        void prioritise(GridPosition cell, const PlatformerTraversalProfile& profile);

        // The cached connections for this cell and profile, or nothing if absent. A
        // climber's cell holds the connections leaving each of its surfaces.
        const std::vector<NavigationConnection>* cachedConnections(
            GridPosition cell,
            const PlatformerTraversalProfile& profile) const;
        // The cached footprint, for showing why a break drops the cell.
        std::optional<CellRange> cachedFootprint(
            GridPosition cell,
            const PlatformerTraversalProfile& profile) const;
        // Stores these as the cell's connections for the profile, replacing any previous value,
        // and returns the stored connections. The footprint is every cell their
        // simulation swept: a break inside it drops them.
        const std::vector<NavigationConnection>& storeConnections(
            GridPosition cell,
            const PlatformerTraversalProfile& profile,
            std::vector<NavigationConnection> connections,
            const CellRange& footprint);
        // A flat-ground walk result keyed by signed cell distance (negative for left).
        // Successful walks start and end at rest, so their costs and relative sweeps
        // can be reused across floors for this profile. Failed attempts are cached too.
        // Returns null when absent.
        const WalkSimulationResult* cachedWalk(
            int columns,
            const PlatformerTraversalProfile& profile) const;
        void storeWalk(const PlatformerTraversalProfile& profile, const WalkSimulationResult& walk);
        void clear();
        // Cells with cached connections, over every profile.
        std::size_t size() const;
        // Every profile anything has been cached or queued for, in the order first met.
        std::vector<PlatformerTraversalProfile> knownProfiles() const;

        // The first three counts are per profile; the rest cover the cache since it was
        // cleared. Cached cells include non-standable cells; connected cells have at least
        // one connection.
        std::size_t cachedCellCount(const PlatformerTraversalProfile& profile) const;
        std::size_t cellsConnected(const PlatformerTraversalProfile& profile) const;
        std::size_t cachedWalkCount(const PlatformerTraversalProfile& profile) const;
        std::size_t breaksApplied() const;
        std::size_t cellsDroppedSoFar() const;
        std::size_t connectionWritesSoFar() const;

    private:
        struct CachedConnections
        {
            std::vector<NavigationConnection> connections;
            CellRange footprint;
        };

        using CellConnections =
            std::unordered_map<GridPosition, CachedConnections, GridPositionHash>;

        struct ProfileCache
        {
            PlatformerTraversalProfile profile;
            CellConnections cells;
            std::unordered_map<int, WalkSimulationResult> walks;
            // The queue, and the same cells as a set so membership is a lookup.
            std::deque<GridPosition> pending;
            std::unordered_set<GridPosition, GridPositionHash> waiting;
        };

        void requireValid(const PlatformerTraversalProfile& profile) const;
        ProfileCache& cacheFor(const PlatformerTraversalProfile& profile);
        ProfileCache* findCacheFor(const PlatformerTraversalProfile& profile);
        const ProfileCache* findCacheFor(const PlatformerTraversalProfile& profile) const;

        std::vector<ProfileCache> profileCaches;
        // Breaks, dropped cells, and connection writes since the cache was cleared.
        std::size_t breaksSeen = 0;
        std::size_t dropsSoFar = 0;
        std::size_t connectionWriteCount = 0;
    };
}
