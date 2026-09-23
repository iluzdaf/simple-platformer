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

    struct GridPositionHash
    {
        std::size_t operator()(GridPosition cell) const;
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
        void clear();
        // Cells kept, over every body.
        std::size_t size() const;

    private:
        using CellConnections =
            std::unordered_map<GridPosition, std::vector<NavigationNeighbor>, GridPositionHash>;

        struct BodyConnections
        {
            ConnectionBody body;
            CellConnections cells;
        };

        std::vector<BodyConnections> bodies;
    };
}
