#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <unordered_map>
#include <unordered_set>
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

    // What a walk of some number of cells along a floor cost when it was simulated, and
    // the cells its simulation swept, as offsets from the cell it started in. No cost
    // when the body could not reach the cell and stop within the simulation limit.
    struct RememberedWalk
    {
        std::optional<int> cost;
        CellRange sweep;
    };

    // What platformer searches learn about one map, kept so nothing is worked out twice:
    // the connections leaving each cell, the cost of a walk of each length, the cells
    // reachable from each start a search failed from, and the path found for each
    // query. All of it depends only on the map, the body and the step, so a cache
    // serves one map. When a tile of that map breaks, the cache drops only what the
    // break can have changed: each cell's connections come with the footprint their
    // simulation swept, and a broken tile inside a footprint drops that cell, the
    // reachable sets that held it, and every remembered path, since a new opening can
    // make a cheaper route anywhere. Walks stay, since no tile decided them. What is
    // learned for one body is kept apart from another's. Every keep requires a body
    // with a finite, positive size and step.
    class PlatformerConnectionCache
    {
    public:
        // Drops what the map's breaks since the last sync can have changed. Every read of
        // the cache that has the map to hand syncs first, so nothing has to remember to.
        void syncWith(const TileMap& map);
        // Drops what a break of this cell can have changed, as syncWith does per break.
        // Every cell dropped joins the body's fill queue.
        void invalidate(GridPosition brokenCell);

        // The cells waiting to be kept, in the order the fill takes them: every cell of
        // the map when a level starts, and the cells a break drops after. Keeping a cell
        // takes it off. A search that needs one before its turn moves it to the front.
        // Queuing a cell that is kept or waiting already changes nothing.
        void queue(GridPosition cell, const ConnectionBody& body);
        std::size_t cellsPending(const ConnectionBody& body) const;
        bool isPending(GridPosition cell, const ConnectionBody& body) const;
        std::optional<GridPosition> nextPending(const ConnectionBody& body) const;
        void prioritise(GridPosition cell, const ConnectionBody& body);

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
        // A walk of this many cells along a floor, negative for leftwards, starts and
        // ends at rest on flat ground, so it costs the same and sweeps the same cells
        // from any cell of any floor: it is simulated once per body and kept here. The
        // record kept, or nothing while none has been.
        const RememberedWalk* walkKept(int columns, const ConnectionBody& body) const;
        void keepWalk(int columns, const ConnectionBody& body, const RememberedWalk& walk);
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

        // Counts for the debug overlay. The first five are for one body; the rest are
        // over every body since the cache was cleared. Every cell asked about is kept,
        // standable or not; the connected ones are those kept with at least one
        // connection.
        std::size_t cellsKept(const ConnectionBody& body) const;
        std::size_t cellsConnected(const ConnectionBody& body) const;
        std::size_t walksKept(const ConnectionBody& body) const;
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
            std::unordered_map<int, RememberedWalk> walks;
            ReachableCells reachable;
            PathsFound paths;
            // The queue, and the same cells as a set so membership is a lookup.
            std::deque<GridPosition> pending;
            std::unordered_set<GridPosition, GridPositionHash> waiting;
        };

        void requireValid(const ConnectionBody& body) const;
        BodyConnections& connectionsFor(const ConnectionBody& body);
        BodyConnections* findConnectionsFor(const ConnectionBody& body);
        const BodyConnections* findConnectionsFor(const ConnectionBody& body) const;

        std::vector<BodyConnections> bodies;
        // How many of the map's breaks have been applied, and what they and the keeps
        // have added up to since the cache was cleared.
        std::size_t breaksSeen = 0;
        std::size_t dropsSoFar = 0;
        std::size_t keepsSoFar = 0;
    };
}
