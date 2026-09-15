#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"

namespace simple_platformer
{
    using GridNeighborFunction = std::function<std::vector<GridPosition>(GridPosition position)>;

    std::optional<std::vector<GridPosition>> findGridPath(
        GridPosition start,
        GridPosition goal,
        const GridNeighborFunction& neighbors);
}
