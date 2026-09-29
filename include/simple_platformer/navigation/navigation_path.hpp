#pragma once

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/input/input_program.hpp"
#include "simple_platformer/navigation/traversal.hpp"

namespace simple_platformer
{
    // Where the body's feet rest at the end of a step, how it gets there, and the
    // inputs recorded for a fall, jump, or climb; a walk or flight has none.
    struct Waypoint
    {
        glm::vec2 feet{0.0F, 0.0F};
        Traversal traversal = Traversal::Fly;
        InputProgram inputs;
    };

    // Where the body's feet rest at the start, and the waypoints that lead from there
    // in the order travelled. No waypoints means the start is the end.
    struct NavigationPath
    {
        glm::vec2 startFeet{0.0F, 0.0F};
        std::vector<Waypoint> waypoints;
    };

    // The last waypoint's feet, or the start of a path without waypoints.
    inline glm::vec2 endOf(const NavigationPath& path)
    {
        return path.waypoints.empty() ? path.startFeet : path.waypoints.back().feet;
    }

    enum class NavigationPathStatus
    {
        Found,
        Unreachable,
        Deferred
    };

    // Found carries a path that ends in the cell holding the target. Unreachable
    // carries a path to the reachable cell nearest it, without waypoints when the
    // actor is already there. Deferred carries no path: the caller should retry after
    // pending navigation work completes. Flying paths never defer.
    struct NavigationPathResult
    {
        NavigationPathStatus status = NavigationPathStatus::Unreachable;
        std::optional<NavigationPath> path;
        // How far the path's last waypoint is from the target, when there is a path.
        float remainingDistance = 0.0F;
    };
}
