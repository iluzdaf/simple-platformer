#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
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

    // The platformer connections leaving each cell, kept once simulated so no search
    // simulates a cell twice. They depend only on the map, the cell and the body, and a
    // map never changes within a level, so a cache serves one map for as long as the
    // level lasts. Connections found for one body are kept apart from another's.
    class PlatformerConnectionCache
    {
    public:
        // The connections kept for this cell and body, or nothing while none have been.
        const std::vector<NavigationNeighbor>* find(GridPosition cell, const ConnectionBody& body)
            const;
        // Keeps these as the cell's connections for the body, replacing any kept before.
        // The body must have a finite, positive size and step.
        void keep(
            GridPosition cell,
            const ConnectionBody& body,
            std::vector<NavigationNeighbor> connections);
        // The cells a body can reach from this start, learned from a search that failed
        // there, or nothing while none has. A goal outside the set has no path, so a
        // search for one need not run.
        const std::vector<GridPosition>* reachableFrom(
            GridPosition start,
            const ConnectionBody& body) const;
        // Keeps these as the cells reachable from the start for the body. The body must
        // have a finite, positive size and step.
        void keepReachable(
            GridPosition start,
            const ConnectionBody& body,
            std::vector<GridPosition> cells);
        // The path an earlier search found for this query and body, or nothing while none
        // has. The connections never change, so neither does the cheapest route.
        const NavigationPath* pathKept(const PathQuery& query, const ConnectionBody& body) const;
        void keepPath(const PathQuery& query, const ConnectionBody& body, NavigationPath path);
        void clear();
        // Cells whose connections are kept, over every body.
        std::size_t size() const;

    private:
        using CellConnections =
            std::unordered_map<GridPosition, std::vector<NavigationNeighbor>, GridPositionHash>;
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
    };
}
