#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/navigation/actor_navigation.hpp"
#include "simple_platformer/navigation/platformer_connection_cache.hpp"
#include "simple_platformer/navigation/route.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/navigation/platformer_cells.hpp"
#include "simple_platformer/navigation/platformer_connections.hpp"
#include "simple_platformer/navigation/platformer_traversal_profile.hpp"
#include "simple_platformer/navigation/traversal.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/actor_builder.hpp"
#include "support/fixed_step.hpp"
#include "support/route_connections.hpp"
#include "support/navigation_paths.hpp"
#include "support/tile_map_builder.hpp"
#include "support/tile_size.hpp"

namespace
{
    using simple_platformer::Cell;
    using simple_platformer::ClimbSurface;
    using simple_platformer::FrameProfile;
    using simple_platformer::NavigationPathResult;
    using simple_platformer::NavigationPathStatus;
    using simple_platformer::PlatformerConnectionCache;
    using simple_platformer::PlatformerTraversalProfile;
    using simple_platformer::RouteConnection;
    using simple_platformer::RouteLocation;
    using simple_platformer::Traversal;

    constexpr glm::vec2 SmallBody{12.0F, 12.0F};
    constexpr glm::vec2 TallBody{12.0F, 20.0F};
    constexpr glm::vec2 FlyerSize{12.0F, 8.0F};

    const PlatformerTraversalProfile Walker{SmallBody, {}, tests::FixedStepSeconds};
    const PlatformerTraversalProfile Climber{SmallBody, {}, tests::FixedStepSeconds, {{60.0F}}};

    glm::vec2 feetIn(Cell cell)
    {
        return simple_platformer::feetInCell(tests::TileSize, cell);
    }

    Cell cellOf(glm::vec2 feet)
    {
        return simple_platformer::cellAtFeet(tests::TileSize, feet);
    }

    // Where a small body's feet rest at the location.
    glm::vec2 feetAt(RouteLocation location)
    {
        return simple_platformer::feetOf(
            simple_platformer::boundsAtSurface(tests::TileSize, location, SmallBody));
    }

    // The search's result, or a failure if the body rested nowhere.
    NavigationPathResult resultOf(const std::optional<NavigationPathResult>& result)
    {
        if (!result.has_value())
        {
            throw std::logic_error("The search found nowhere to start from");
        }
        return *result;
    }

    simple_platformer::NavigationPath pathOf(const NavigationPathResult& result)
    {
        return result.path.value_or(simple_platformer::NavigationPath{});
    }

    glm::vec2 endOf(const NavigationPathResult& result)
    {
        return simple_platformer::endOf(pathOf(result));
    }

    std::optional<NavigationPathResult> searchWith(
        const simple_platformer::TileMap& map,
        const simple_platformer::Aabb& body,
        glm::vec2 target,
        const PlatformerTraversalProfile& profile,
        PlatformerConnectionCache& cache,
        FrameProfile* frameProfile = nullptr)
    {
        return simple_platformer::findActorPath(
            map, tests::actorFor(body, profile), target, profile.stepSeconds, cache, frameProfile);
    }

    // A search once the fill has cached every cell of the map for the profile, as the
    // game's world does for each NPC.
    NavigationPathResult findPath(
        const simple_platformer::TileMap& map,
        const simple_platformer::Aabb& body,
        glm::vec2 target,
        const PlatformerTraversalProfile& profile,
        FrameProfile* frameProfile = nullptr)
    {
        PlatformerConnectionCache cache;
        tests::fillConnections(map, cache, profile);
        return resultOf(searchWith(map, body, target, profile, cache, frameProfile));
    }

    // Where a search from this body starts: the resting feet it chose, or nothing when
    // the body rests nowhere.
    std::optional<glm::vec2> startFeetFrom(
        const simple_platformer::TileMap& map,
        const simple_platformer::Aabb& body,
        const PlatformerTraversalProfile& profile)
    {
        PlatformerConnectionCache cache;
        tests::fillConnections(map, cache, profile);
        const std::optional<NavigationPathResult> result =
            searchWith(map, body, simple_platformer::feetOf(body), profile, cache);
        if (!result.has_value() || !result->path.has_value())
        {
            return std::nullopt;
        }
        return result->path->startFeet;
    }

    std::optional<NavigationPathResult> findFlight(
        const simple_platformer::TileMap& map,
        Cell cell,
        glm::vec2 target,
        FrameProfile* frameProfile = nullptr)
    {
        const simple_platformer::Actor flyer =
            tests::ActorBuilder::sized(FlyerSize).inCell(cell).flying(60.0F);
        PlatformerConnectionCache unused;
        return simple_platformer::findActorPath(
            map, flyer, target, tests::FixedStepSeconds, unused, frameProfile);
    }

    bool hasStep(const simple_platformer::NavigationPath& route, Traversal traversal)
    {
        return std::any_of(
            route.waypoints.begin(),
            route.waypoints.end(),
            [traversal](const simple_platformer::Waypoint& waypoint)
            { return waypoint.traversal == traversal; });
    }

    simple_platformer::TileMap climbableWall()
    {
        return tests::TileMapBuilder({"......", ".c....", ".c....", ".c....", ".c....", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    }

    // Two floors either side of a block, joined only over its climbable walls and ceiling.
    simple_platformer::TileMap climbOverBlock()
    {
        return tests::TileMapBuilder({"..............",
                                      "..cccccccccc..",
                                      ".c..........c.",
                                      ".c.########.c.",
                                      ".c.########.c.",
                                      ".c.########.c.",
                                      "..##########..",
                                      ".............."})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    }

    // Moves a body along the path with the real follower and movement, as an NPC is.
    bool followsToTheEnd(
        const simple_platformer::TileMap& map,
        const simple_platformer::NavigationPath& path,
        simple_platformer::Body body,
        simple_platformer::SurfaceClimb& climb,
        int tickLimit)
    {
        simple_platformer::PathFollower follower;
        simple_platformer::setPath(follower, path);
        simple_platformer::PlatformerMovement movement{
            Climber.movement, climb.surface == ClimbSurface::None};
        for (int tick = 0; tick < tickLimit && !simple_platformer::pathComplete(follower); ++tick)
        {
            const simple_platformer::InputIntentions intentions =
                simple_platformer::followPlatformerPath(
                    body, movement, follower, Climber.stepSeconds, &climb);
            simple_platformer::updateSurfaceClimbMovement(
                map, body, movement, climb, intentions, Climber.stepSeconds);
        }
        return simple_platformer::pathComplete(follower);
    }
}

// The entry point

TEST_CASE("Actor navigation searches with the actor's capabilities", "[navigation][actor]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..............",
                               "..cccccccccc..",
                               ".c..........c.",
                               ".c.########.c.",
                               ".c.########.c.",
                               ".c.########.c.",
                               "..##########..",
                               ".............."})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    const glm::vec2 startFeet = simple_platformer::feetInCell(tests::TileSize, {2, 5});
    const glm::vec2 goalFeet = simple_platformer::feetInCell(tests::TileSize, {11, 5});
    simple_platformer::Actor actor =
        tests::ActorBuilder::sized({12.0F, 12.0F}).atFeet(startFeet).walking();

    const auto searchAs = [&map, &goalFeet](const simple_platformer::Actor& navigator)
    {
        PlatformerConnectionCache cache;
        tests::fillConnections(
            map,
            cache,
            simple_platformer::platformerTraversalProfileFor(navigator, tests::FixedStepSeconds));
        return resultOf(simple_platformer::findActorPath(
            map, navigator, goalFeet, tests::FixedStepSeconds, cache));
    };

    REQUIRE(searchAs(actor).status == simple_platformer::NavigationPathStatus::Unreachable);

    actor.surfaceClimb = simple_platformer::SurfaceClimb{{60.0F}};
    const NavigationPathResult climbing = searchAs(actor);
    REQUIRE(climbing.status == simple_platformer::NavigationPathStatus::Found);
    REQUIRE(climbing.path.has_value());
    REQUIRE(hasStep(pathOf(climbing), Traversal::Climb));
}

TEST_CASE("Actor navigation starts from where the body rests", "[navigation][actor]")
{
    using simple_platformer::ClimbSurface;
    using simple_platformer::RouteLocation;
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"cccccc", "c.....", "c.....", "c.....", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    const glm::vec2 bodySize{12.0F, 12.0F};
    const glm::vec2 floorFeet = simple_platformer::feetInCell(tests::TileSize, {4, 3});
    simple_platformer::Actor actor =
        tests::ActorBuilder::sized(bodySize).inCell({3, 1}).walking().climbing({60.0F});
    PlatformerConnectionCache cache;
    tests::fillConnections(
        map,
        cache,
        simple_platformer::platformerTraversalProfileFor(actor, tests::FixedStepSeconds));

    // In the air there is nowhere to start from.
    REQUIRE_FALSE(
        simple_platformer::findActorPath(map, actor, floorFeet, tests::FixedStepSeconds, cache)
            .has_value());

    // Against the ceiling, the body rests there; the climb state is not consulted.
    const RouteLocation hanging{{3, 1}, ClimbSurface::Ceiling};
    actor.body.bounds = simple_platformer::boundsAtSurface(tests::TileSize, hanging, bodySize);
    REQUIRE(actor.surfaceClimb.has_value());
    REQUIRE(
        actor.surfaceClimb.value_or(simple_platformer::SurfaceClimb{}).surface ==
        ClimbSurface::None);
    const NavigationPathResult held = resultOf(
        simple_platformer::findActorPath(map, actor, floorFeet, tests::FixedStepSeconds, cache));
    REQUIRE(held.status == simple_platformer::NavigationPathStatus::Found);
    REQUIRE(held.path.has_value());
    REQUIRE(pathOf(held).startFeet == simple_platformer::feetOf(actor.body.bounds));
}

TEST_CASE("A walker starts from the standable cell that supports it", "[navigation][actor]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "..###..."});
    const auto startFrom = [&map](const simple_platformer::Aabb& body)
    { return startFeetFrom(map, body, {body.size, {}, tests::FixedStepSeconds}); };

    REQUIRE(
        startFrom(simple_platformer::boxInCell(tests::TileSize, {3, 1}, TallBody)) ==
        feetIn({3, 1}));
    // A body wider than a tile is still placed by its feet.
    REQUIRE(
        startFrom(simple_platformer::boxInCell(tests::TileSize, {3, 1}, {20.0F, 20.0F})) ==
        feetIn({3, 1}));

    // At the ledge the feet hang past the platform, so the cell under them cannot be
    // stood on; the body starts from the supporting cell.
    simple_platformer::Aabb atTheLedge{{0.0F, 0.0F}, TallBody};
    simple_platformer::placeFeetAt(atTheLedge, {80.5F, 32.0F});
    REQUIRE(cellOf(simple_platformer::feetOf(atTheLedge)) == Cell{5, 1});
    REQUIRE(startFrom(atTheLedge) == feetIn({4, 1}));

    // In the air, or a little above the floor, it rests nowhere and gets no result.
    REQUIRE_FALSE(startFrom(simple_platformer::boxInCell(tests::TileSize, {0, 0}, TallBody)));
    simple_platformer::Aabb hovering =
        simple_platformer::boxInCell(tests::TileSize, {3, 1}, TallBody);
    hovering.position.y -= 3.0F;
    REQUIRE_FALSE(startFrom(hovering));
}

TEST_CASE("A climber starts from the surface its body is against", "[navigation][actor][climb]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"cccccc", "c.....", "c.....", "c.....", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());

    // Partway up the wall, from the nearest resting place along it.
    simple_platformer::Aabb onWall = simple_platformer::boundsAtSurface(
        tests::TileSize, {{1, 2}, ClimbSurface::LeftWall}, SmallBody);
    onWall.position.y -= 5.0F;
    REQUIRE(startFeetFrom(map, onWall, Climber) == feetAt({{1, 2}, ClimbSurface::LeftWall}));
    // A walker cannot hold the wall, so the same body rests nowhere.
    REQUIRE_FALSE(startFeetFrom(map, onWall, Walker));

    const RouteLocation hanging{{3, 1}, ClimbSurface::Ceiling};
    REQUIRE(
        startFeetFrom(
            map,
            simple_platformer::boundsAtSurface(tests::TileSize, hanging, SmallBody),
            Climber) == feetAt(hanging));
}

TEST_CASE("Traversal profiles distinguish optional climbing capabilities", "[navigation][actor]")
{
    simple_platformer::Actor actor =
        tests::ActorBuilder::sized({12.0F, 12.0F}).inCell({0, 0}).walking();
    const auto walking =
        simple_platformer::platformerTraversalProfileFor(actor, tests::FixedStepSeconds);
    REQUIRE_FALSE(walking.climb.has_value());

    actor.surfaceClimb = simple_platformer::SurfaceClimb{{60.0F}};
    const auto climbing =
        simple_platformer::platformerTraversalProfileFor(actor, tests::FixedStepSeconds);
    REQUIRE(climbing.climb.has_value());
    REQUIRE(climbing.climb.value_or(simple_platformer::SurfaceClimbConfig{}).speed == 60.0F);
    REQUIRE_FALSE(climbing == walking);

    actor.surfaceClimb = simple_platformer::SurfaceClimb{{90.0F}};
    REQUIRE_FALSE(
        simple_platformer::platformerTraversalProfileFor(actor, tests::FixedStepSeconds) ==
        climbing);
}

// Flying

TEST_CASE("A flying path crosses open cells around a wall", "[navigation][flying]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"....", ".##.", "...."});

    simple_platformer::FrameProfile profile;
    const simple_platformer::NavigationPathResult result = resultOf(
        findFlight(map, {0, 1}, simple_platformer::feetInCell(tests::TileSize, {3, 1}), &profile));

    REQUIRE(result.status == simple_platformer::NavigationPathStatus::Found);
    REQUIRE(result.path.has_value());
    const simple_platformer::NavigationPath route = pathOf(result);
    REQUIRE(cellOf(route.startFeet) == simple_platformer::Cell{0, 1});
    REQUIRE(cellOf(route.waypoints.back().feet) == simple_platformer::Cell{3, 1});
    REQUIRE(route.waypoints.front().traversal == simple_platformer::Traversal::Fly);
    // A flying search expands cells but simulates no movement.
    REQUIRE(simple_platformer::frameStatisticCount(profile, "Cells expanded") >= 1);
}

TEST_CASE("A flyer whose feet are off the map gets no result", "[navigation][flying]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", "..."});
    REQUIRE_FALSE(findFlight(map, {3, 0}, simple_platformer::feetInCell(tests::TileSize, {0, 0}))
                      .has_value());
}

// Platformers

TEST_CASE(
    "The jump start penalty stops a needless hop but not a needed jump",
    "[navigation][platformer][regression]")
{
    // A platform along the way that a hop over would save a few ticks, not worth the
    // penalty every jump start is charged.
    const simple_platformer::TileMap hop =
        tests::TileMapBuilder({".....###.....", ".............", ".............", "#############"});
    simple_platformer::PlatformerMovementConfig movement;
    movement.maximumSpeed = 60.0F;
    const simple_platformer::NavigationPath preferred = pathOf(findPath(
        hop,
        tests::restingBody({{12, 2}}, {TallBody, movement, tests::FixedStepSeconds}),
        feetIn({4, 2}),
        {TallBody, movement, tests::FixedStepSeconds}));
    REQUIRE_FALSE(preferred.waypoints.empty());
    REQUIRE_FALSE(hasStep(preferred, Traversal::Jump));

    // A goal on a platform is reached only by jumping, penalty or not.
    const simple_platformer::TileMap platform =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const simple_platformer::PlatformerMovementConfig ordinary;
    const simple_platformer::PlatformerTraversalProfile profile{
        SmallBody, ordinary, tests::FixedStepSeconds};
    const std::vector<RouteConnection> connections =
        simple_platformer::buildPlatformerConnections(platform, {2, 2}, profile).connections;
    const RouteConnection& up = tests::jumpUpFrom(connections, 2);
    const simple_platformer::NavigationPath climbed = pathOf(findPath(
        platform,
        tests::restingBody({{2, 2}}, profile),
        feetIn(up.step.destination.cell),
        profile));
    REQUIRE(hasStep(climbed, Traversal::Jump));
}

TEST_CASE("Platformer search rejects an invalid step", "[navigation][platformer][validation]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...", "###"});
    const simple_platformer::PlatformerMovementConfig movement;
    for (const float step :
         {0.0F, -tests::FixedStepSeconds, std::numeric_limits<float>::infinity()})
    {
        REQUIRE_THROWS_AS(
            findPath(
                map,
                tests::restingBody({{0, 0}}, {SmallBody, movement, step}),
                feetIn({1, 0}),
                {SmallBody, movement, step}),
            std::invalid_argument);
    }
}

TEST_CASE("Platformer searches report what they cost", "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const simple_platformer::PlatformerMovementConfig movement;

    const simple_platformer::PlatformerTraversalProfile profile{
        SmallBody, movement, tests::FixedStepSeconds};

    simple_platformer::FrameProfile searchCost;
    const std::optional<simple_platformer::NavigationPath> path =
        findPath(map, tests::restingBody({{2, 2}}, profile), feetIn({7, 2}), profile, &searchCost)
            .path;
    REQUIRE(path.has_value());
    // The search expands at least its start cell.
    REQUIRE(simple_platformer::frameStatisticCount(searchCost, "Path searches") == 1);
    REQUIRE(simple_platformer::frameStatisticCount(searchCost, "Cells expanded") >= 1);
}

TEST_CASE(
    "An unreachable target reports the distance left from the path's end",
    "[navigation][platformer]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "########"});
    const glm::vec2 midJump = feetIn({5, 1}) - glm::vec2{0.0F, 4.0F};

    const NavigationPathResult result =
        findPath(map, tests::restingBody({{1, 2}}, Walker), midJump, Walker);

    // No body resting anywhere reaches the point, so the path stops as close as it can.
    REQUIRE(result.status == NavigationPathStatus::Unreachable);
    REQUIRE(result.path.has_value());
    REQUIRE(endOf(result) == feetAt({{5, 2}}));
    REQUIRE(result.remainingDistance > 0.0F);
}

// Climbing

TEST_CASE(
    "A path begun partway along a wall reaches its recorded start",
    "[navigation][platformer][climb]")
{
    const simple_platformer::TileMap map = climbableWall();
    const RouteLocation start{{2, 3}, ClimbSurface::LeftWall};
    const NavigationPathResult result = findPath(
        map, tests::restingBody(start, Climber), feetAt({{2, 2}, ClimbSurface::LeftWall}), Climber);
    REQUIRE(result.path.has_value());

    simple_platformer::Body body{
        simple_platformer::boundsAtSurface(tests::TileSize, start, SmallBody), {0.0F, 0.0F}};
    body.bounds.position.y -= 6.0F;
    simple_platformer::SurfaceClimb climb{{60.0F}, ClimbSurface::LeftWall};
    REQUIRE(followsToTheEnd(map, pathOf(result), body, climb, 120));
}

TEST_CASE(
    "One route crosses a floor, wall, ceiling, and another floor",
    "[navigation][platformer][climb]")
{
    const simple_platformer::TileMap map = climbOverBlock();
    const RouteLocation start{{2, 5}};

    const NavigationPathResult walking =
        findPath(map, tests::restingBody(start, Walker), feetIn({11, 5}), Walker);
    REQUIRE(walking.status == NavigationPathStatus::Unreachable);

    simple_platformer::FrameProfile searchCost;
    const NavigationPathResult result =
        findPath(map, tests::restingBody(start, Climber), feetIn({11, 5}), Climber, &searchCost);
    REQUIRE(result.status == NavigationPathStatus::Found);
    REQUIRE(result.path.has_value());
    const simple_platformer::NavigationPath route = pathOf(result);
    const auto passes = [&route](RouteLocation location)
    {
        return std::any_of(
            route.waypoints.begin(),
            route.waypoints.end(),
            [location](const simple_platformer::Waypoint& waypoint)
            { return waypoint.feet == feetAt(location); });
    };
    REQUIRE(passes({{2, 3}, ClimbSurface::LeftWall}));
    REQUIRE(passes({{6, 2}, ClimbSurface::Ceiling}));
    REQUIRE(passes({{11, 3}, ClimbSurface::RightWall}));
    // Holding the wall at the foot of the far side is already in the target's cell.
    REQUIRE(
        simple_platformer::cellAtFeet(tests::TileSize, endOf(result)) ==
        simple_platformer::Cell{11, 5});

    simple_platformer::Body body{
        simple_platformer::boundsAtSurface(tests::TileSize, start, SmallBody), {0.0F, 0.0F}};
    simple_platformer::SurfaceClimb climb{{60.0F}};
    REQUIRE(followsToTheEnd(map, pathOf(result), body, climb, 2000));
}

// Searching with the connection cache

TEST_CASE("A search never simulates or writes to the cache", "[navigation][cache]")
{
    const simple_platformer::TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "###..###", "########"});
    const RouteLocation start{{0, 2}};
    const glm::vec2 goal = feetIn({7, 2});
    PlatformerConnectionCache cache;

    // An empty cache: the search stores nothing, queues the cell it needs at the front
    // of the fill, and defers.
    FrameProfile waiting;
    const NavigationPathResult deferred =
        resultOf(searchWith(map, tests::restingBody(start, Walker), goal, Walker, cache, &waiting));
    REQUIRE(deferred.status == NavigationPathStatus::Deferred);
    REQUIRE_FALSE(deferred.path.has_value());
    REQUIRE(cache.cachedCellCount(Walker) == 0);
    REQUIRE(cache.connectionWritesSoFar() == 0);
    REQUIRE(cache.cellsPending(Walker) == 1);
    REQUIRE(cache.nextPending(Walker) == start.cell);
    REQUIRE(simple_platformer::frameStatisticCount(waiting, "Cells expanded") == 0);
    REQUIRE(simple_platformer::frameStatisticCount(waiting, "Paths deferred") == 1);

    // Once the fill has cached the map, the search reads it and still writes nothing.
    tests::fillConnections(map, cache, Walker);
    const std::size_t writesBeforeSearching = cache.connectionWritesSoFar();
    FrameProfile reading;
    const NavigationPathResult found =
        resultOf(searchWith(map, tests::restingBody(start, Walker), goal, Walker, cache, &reading));
    REQUIRE(found.status == NavigationPathStatus::Found);
    REQUIRE(simple_platformer::frameStatisticCount(reading, "Cells expanded") > 0);
    REQUIRE(cache.connectionWritesSoFar() == writesBeforeSearching);
}

TEST_CASE(
    "A pending cell defers a search even when an expensive route is available",
    "[navigation][cache]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"...."});
    const Cell start{0, 0};
    const Cell pending{1, 0};
    const Cell goal{2, 0};
    const Cell unrelated{3, 0};
    const PlatformerTraversalProfile profile{SmallBody, {}, tests::FixedStepSeconds};
    PlatformerConnectionCache cache;
    cache.storeConnections(
        start,
        profile,
        {{{{pending}, simple_platformer::Traversal::Walk, {}}, 1},
         {{{goal}, simple_platformer::Traversal::Walk, {}}, 100}},
        {start, goal});
    cache.queue(unrelated, profile);
    cache.queue(pending, profile);
    REQUIRE(cache.nextPending(profile) == unrelated);

    FrameProfile waiting;
    const auto deferred = resultOf(searchWith(
        map, tests::restingBody({start}, profile), feetIn(goal), profile, cache, &waiting));
    REQUIRE(deferred.status == simple_platformer::NavigationPathStatus::Deferred);
    REQUIRE_FALSE(deferred.path.has_value());
    REQUIRE(simple_platformer::frameStatisticCount(waiting, "Paths deferred") == 1);
    REQUIRE(simple_platformer::frameStatisticCount(waiting, "Cells expanded") == 1);
    REQUIRE(cache.nextPending(profile) == pending);

    cache.storeConnections(
        pending, profile, {{{{goal}, simple_platformer::Traversal::Walk, {}}, 1}}, {pending, goal});
    const auto found = resultOf(
        searchWith(map, tests::restingBody({start}, profile), feetIn(goal), profile, cache));
    REQUIRE(found.status == simple_platformer::NavigationPathStatus::Found);
    REQUIRE(found.path.has_value());
    const simple_platformer::NavigationPath route = pathOf(found);
    REQUIRE(route.waypoints.size() == 2);
    REQUIRE(route.waypoints.front().feet == feetIn(pending));
}
