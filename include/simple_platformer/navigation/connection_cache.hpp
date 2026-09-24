#pragma once

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    class TileMap;

    // What a cell's connections are simulated for. The same body at the same step finds
    // the same connections from a cell every time.
    struct ConnectionBody
    {
        glm::vec2 size = {0.0F, 0.0F};
        PlatformerMovementConfig movement;
        float stepSeconds = 0.0F;
    };

    bool operator==(const ConnectionBody& left, const ConnectionBody& right);

    // What a path was searched for: from where to where, and the jump start penalty the
    // search charged, which can change which route is cheapest.
    struct PathQuery
    {
        GridPosition start;
        GridPosition goal;
        int jumpStartPenaltyTicks = 0;
    };

    bool operator==(const PathQuery& left, const PathQuery& right);

    struct PathQueryHash
    {
        std::size_t operator()(const PathQuery& query) const;
    };

    // What platformer searches learn about one map, kept so nothing is worked out twice:
    // the connections leaving each cell, the cells reachable from each start a search
    // failed from, and the path found for each query. All of it depends only on the
    // map, the body and the step, so a cache serves one map. When a tile of that map
    // breaks, the cache drops only what the break can have changed: each cell's
    // connections come with the footprint their simulation swept, and a broken tile
    // inside a footprint drops that cell, the reachable sets that held it, and every
    // remembered path, since a new opening can make a cheaper route anywhere. What is
    // learned for one body is kept apart from another's. Every keep requires a body
    // with a finite, positive size and step.
    class PlatformerConnectionCache
    {
    public:
        // Drops what the map's breaks since the last sync can have changed. Every read of
        // the cache that has the map to hand syncs first, so nothing has to remember to.
        void syncWith(const TileMap& map);
        // Drops what a break of this cell can have changed, as syncWith does per break.
        void invalidate(GridPosition brokenCell);

        // The connections kept for this cell and body, or nothing while none have been.
        const std::vector<NavigationNeighbor>* find(GridPosition cell, const ConnectionBody& body)
            const;
        // The footprint kept with them, for showing why a break drops the cell.
        std::optional<CellRange> footprintKept(GridPosition cell, const ConnectionBody& body) const;
        // Keeps these as the cell's connections for the body, replacing any kept before,
        // and returns them where they are kept. The footprint is every cell their
        // simulation swept: a break inside it drops them.
        const std::vector<NavigationNeighbor>& keep(
            GridPosition cell,
            const ConnectionBody& body,
            std::vector<NavigationNeighbor> connections,
            const CellRange& footprint);
        // The cells a body can reach from this start, learned from a search that failed
        // there, or nothing while none has. A goal outside the set has no path, so a
        // search for one need not run.
        const std::vector<GridPosition>* reachableFrom(
            GridPosition start,
            const ConnectionBody& body) const;
        // Keeps these as the cells reachable from the start for the body.
        void keepReachable(
            GridPosition start,
            const ConnectionBody& body,
            std::vector<GridPosition> cells);
        // The path an earlier search found for this query and body, or nothing while none
        // has. While the connections hold, so does the cheapest route.
        const NavigationPath* pathKept(const PathQuery& query, const ConnectionBody& body) const;
        void keepPath(const PathQuery& query, const ConnectionBody& body, NavigationPath path);
        void clear();
        // Cells whose connections are kept, over every body.
        std::size_t size() const;

        // Counts for the debug overlay. The first four are for one body; the rest are
        // over every body since the cache was cleared, with cells kept including warm-up.
        // Every cell asked about is kept, standable or not; the connected ones are those
        // kept with at least one connection.
        std::size_t cellsKept(const ConnectionBody& body) const;
        std::size_t cellsConnected(const ConnectionBody& body) const;
        std::size_t reachableSetsKept(const ConnectionBody& body) const;
        std::size_t pathsKept(const ConnectionBody& body) const;
        std::size_t breaksApplied() const;
        std::size_t cellsDroppedSoFar() const;
        std::size_t cellsKeptSoFar() const;

    private:
        struct KeptConnections
        {
            std::vector<NavigationNeighbor> connections;
            CellRange footprint;
        };

        using CellConnections = std::unordered_map<GridPosition, KeptConnections, GridPositionHash>;
        using ReachableCells =
            std::unordered_map<GridPosition, std::vector<GridPosition>, GridPositionHash>;
        using PathsFound = std::unordered_map<PathQuery, NavigationPath, PathQueryHash>;

        struct BodyConnections
        {
            ConnectionBody body;
            CellConnections cells;
            ReachableCells reachable;
            PathsFound paths;
        };

        void requireValid(const ConnectionBody& body) const;
        BodyConnections& connectionsFor(const ConnectionBody& body);
        const BodyConnections* findConnectionsFor(const ConnectionBody& body) const;

        std::vector<BodyConnections> bodies;
        // How many of the map's breaks have been applied, and what they and the keeps
        // have added up to since the cache was cleared.
        std::size_t breaksSeen = 0;
        std::size_t dropsSoFar = 0;
        std::size_t keepsSoFar = 0;
    };
}
