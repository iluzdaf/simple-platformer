#include "simple_platformer/navigation/navigation_fill.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/connection_cache.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/platformer_navigation.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    std::vector<ConnectionBody> platformerBodiesIn(const World& world, float stepSeconds)
    {
        if (!isFinitePositive(stepSeconds))
        {
            throw std::invalid_argument("Navigation step must be finite and positive");
        }
        std::vector<ConnectionBody> bodies;
        for (const Actor& actor : world.actors())
        {
            if (!actor.pathFollower.has_value() || !actor.platformerMovement.has_value())
            {
                continue;
            }
            const ConnectionBody body{
                actor.body.bounds.size, actor.platformerMovement->config, stepSeconds};
            const bool known = std::any_of(
                bodies.begin(),
                bodies.end(),
                [&body](const ConnectionBody& kept) { return kept == body; });
            if (!known)
            {
                bodies.push_back(body);
            }
        }
        return bodies;
    }

    void queueNavigation(
        const TileMap& map,
        const std::vector<ConnectionBody>& bodies,
        PlatformerConnectionCache& cache)
    {
        for (const ConnectionBody& body : bodies)
        {
            queueAllPlatformerConnections(map, body.size, body.movement, body.stepSeconds, cache);
        }
    }

    FillWork fillNavigation(const TileMap& map, PlatformerConnectionCache& cache, int tickBudget)
    {
        if (tickBudget < 0)
        {
            throw std::invalid_argument("A fill budget cannot be negative");
        }
        cache.syncWith(map);
        const std::vector<ConnectionBody> bodies = cache.bodiesKept();
        // The step's budget is shared among the bodies with cells waiting; the others
        // are asked anyway, since a fill with nothing waiting costs nothing.
        const auto waiting = static_cast<int>(std::count_if(
            bodies.begin(),
            bodies.end(),
            [&cache](const ConnectionBody& body) { return cache.cellsPending(body) > 0; }));
        const int budgetEach = tickBudget / std::max(1, waiting);
        FillWork total;
        for (const ConnectionBody& body : bodies)
        {
            const FillWork work = fillPlatformerConnections(
                map, body.size, body.movement, body.stepSeconds, cache, budgetEach);
            total.cells += work.cells;
            total.simulatedTicks += work.simulatedTicks;
            total.budgetSpent += work.budgetSpent;
        }
        return total;
    }
}
