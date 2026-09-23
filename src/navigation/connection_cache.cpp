#include "simple_platformer/navigation/connection_cache.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
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

    std::size_t GridPositionHash::operator()(GridPosition cell) const
    {
        const auto column = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell.x));
        const auto row = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell.y));
        return std::hash<std::uint64_t>{}((column << 32U) | row);
    }

    const std::vector<NavigationNeighbor>* PlatformerConnectionCache::find(
        GridPosition cell,
        const ConnectionBody& body) const
    {
        for (const BodyConnections& kept : bodies)
        {
            if (!(kept.body == body))
            {
                continue;
            }
            const auto connections = kept.cells.find(cell);
            return connections == kept.cells.end() ? nullptr : &connections->second;
        }
        return nullptr;
    }

    void PlatformerConnectionCache::keep(
        GridPosition cell,
        const ConnectionBody& body,
        std::vector<NavigationNeighbor> connections)
    {
        if (!isFinite(body.size) || body.size.x <= 0.0F || body.size.y <= 0.0F ||
            !isFinitePositive(body.stepSeconds))
        {
            throw std::invalid_argument(
                "Connections are kept for a finite, positive body size and step");
        }
        auto kept = std::find_if(
            bodies.begin(),
            bodies.end(),
            [&body](const BodyConnections& candidate) { return candidate.body == body; });
        if (kept == bodies.end())
        {
            bodies.push_back({body, {}});
            kept = bodies.end() - 1;
        }
        kept->cells[cell] = std::move(connections);
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
