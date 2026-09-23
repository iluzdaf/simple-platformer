#include "simple_platformer/navigation/connection_cache.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    bool operator==(const ConnectionBody& left, const ConnectionBody& right)
    {
        return left.size == right.size && left.movement == right.movement &&
               left.stepSeconds == right.stepSeconds;
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
        bodies.push_back({body, {}, {}});
        return bodies.back();
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
        return connections == kept->cells.end() ? nullptr : &connections->second;
    }

    void PlatformerConnectionCache::keep(
        GridPosition cell,
        const ConnectionBody& body,
        std::vector<NavigationNeighbor> connections)
    {
        requireValid(body);
        connectionsFor(body).cells[cell] = std::move(connections);
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

    void PlatformerConnectionCache::clear()
    {
        bodies.clear();
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
