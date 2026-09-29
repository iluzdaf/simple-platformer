#include "simple_platformer/navigation/navigation_fill.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/actor_navigation.hpp"
#include "simple_platformer/navigation/platformer_connection_cache.hpp"
#include "simple_platformer/navigation/platformer_connections.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        // Account for cache writes even when a cell needs no movement simulation.
        constexpr int CacheWriteCostTicks = 3;

        int fillPlatformerConnections(
            const TileMap& map,
            const PlatformerTraversalProfile& profile,
            PlatformerConnectionCache& cache,
            int tickBudget,
            FrameProfile* frameProfile)
        {
            int cellsCached = 0;
            int simulatedTicks = 0;
            int budgetSpent = 0;
            while (budgetSpent < tickBudget)
            {
                const std::optional<Cell> next = cache.nextPending(profile);
                if (!next.has_value())
                {
                    break;
                }
                const Cell cell = next.value();
                BuiltPlatformerConnections built =
                    buildPlatformerConnections(map, cell, profile, &cache);
                const int ticksForCell = built.simulatedTicks;
                storePlatformerConnections(cache, cell, profile, std::move(built));
                ++cellsCached;
                simulatedTicks += ticksForCell;
                budgetSpent += ticksForCell + CacheWriteCostTicks;
            }
            addFrameStatistic(frameProfile, "Navigation", "Fill cells cached", cellsCached);
            addFrameStatistic(frameProfile, "Navigation", "Fill simulated ticks", simulatedTicks);
            addFrameStatistic(frameProfile, "Navigation", "Fill budget spent", budgetSpent);
            return cellsCached;
        }

        std::vector<PlatformerTraversalProfile> platformerTraversalProfilesIn(
            const World& world,
            float stepSeconds)
        {
            requirePositiveSeconds(stepSeconds, "Navigation step");
            std::vector<PlatformerTraversalProfile> profiles;
            for (const Actor& actor : world.actors())
            {
                if (!actor.pathFollower.has_value() || !actor.platformerMovement.has_value())
                {
                    continue;
                }
                const PlatformerTraversalProfile profile =
                    platformerTraversalProfileFor(actor, stepSeconds);
                const bool known = std::any_of(
                    profiles.begin(),
                    profiles.end(),
                    [&profile](const PlatformerTraversalProfile& existing)
                    { return existing == profile; });
                if (!known)
                {
                    profiles.push_back(profile);
                }
            }
            return profiles;
        }
    }

    void queueNavigationFill(const TileMap& map, World& world, float stepSeconds)
    {
        const std::vector<PlatformerTraversalProfile> profiles =
            platformerTraversalProfilesIn(world, stepSeconds);
        PlatformerConnectionCache& cache = world.platformerConnections();
        cache.applyRecordedTileBreaks(map);
        for (const PlatformerTraversalProfile& profile : profiles)
        {
            for (int row = 0; row < map.height(); ++row)
            {
                for (int column = 0; column < map.width(); ++column)
                {
                    cache.queue({column, row}, profile);
                }
            }
        }
    }

    int advanceNavigationFill(
        const TileMap& map,
        PlatformerConnectionCache& cache,
        int tickBudget,
        FrameProfile* frameProfile)
    {
        if (tickBudget < 0)
        {
            throw std::invalid_argument("A fill budget cannot be negative");
        }
        cache.applyRecordedTileBreaks(map);
        const std::vector<PlatformerTraversalProfile> profiles = cache.knownProfiles();
        // The step's budget is shared among profiles with cells waiting. A profile
        // without pending cells costs nothing.
        const auto waiting = static_cast<int>(std::count_if(
            profiles.begin(),
            profiles.end(),
            [&cache](const PlatformerTraversalProfile& profile)
            { return cache.cellsPending(profile) > 0; }));
        const int budgetEach = tickBudget / std::max(1, waiting);
        int cellsCached = 0;
        for (const PlatformerTraversalProfile& profile : profiles)
        {
            cellsCached += fillPlatformerConnections(map, profile, cache, budgetEach, frameProfile);
        }
        return cellsCached;
    }
}
