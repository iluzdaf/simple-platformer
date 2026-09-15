#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    using GridNeighborFunction =
        std::function<std::vector<NavigationNeighbor>(GridPosition position)>;

    std::optional<NavigationPath> findGridPath(
        GridPosition start,
        GridPosition goal,
        const GridNeighborFunction& neighbors);
}
