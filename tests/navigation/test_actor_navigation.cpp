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
    using simple_platformer::Actor;
    using simple_platformer::boundsAtSurface;
    using simple_platformer::buildPlatformerConnections;
    using simple_platformer::Cell;
    using simple_platformer::cellAtFeet;
    using simple_platformer::ClimbSurface;
    using simple_platformer::endOf;
    using simple_platformer::feetInCell;
    using simple_platformer::feetOf;
    using simple_platformer::findActorPath;
    using simple_platformer::followPlatformerPath;
    using simple_platformer::FrameProfile;
    using simple_platformer::frameStatisticCount;
    using simple_platformer::InputIntentions;
    using simple_platformer::NavigationPath;
    using simple_platformer::NavigationPathResult;
    using simple_platformer::NavigationPathStatus;
    using simple_platformer::pathComplete;
    using simple_platformer::PathFollower;
    using simple_platformer::PlatformerConnectionCache;
    using simple_platformer::PlatformerMovement;
    using simple_platformer::PlatformerMovementConfig;
    using simple_platformer::PlatformerTraversalProfile;
    using simple_platformer::platformerTraversalProfileFor;
    using simple_platformer::RouteConnection;
    using simple_platformer::RouteLocation;
    using simple_platformer::setPath;
    using simple_platformer::SurfaceClimb;
    using simple_platformer::SurfaceClimbConfig;
    using simple_platformer::TileMap;
    using simple_platformer::Traversal;
    using simple_platformer::updateSurfaceClimbMovement;
    using simple_platformer::Waypoint;

    constexpr glm::vec2 SmallBody{12.0F, 12.0F};
    constexpr glm::vec2 TallBody{12.0F, 20.0F};
    constexpr glm::vec2 FlyerSize{12.0F, 8.0F};
    constexpr SurfaceClimbConfig ClimbConfig{60.0F};

    // A small platformer, which walks, jumps and falls, resting at the location.
    Actor platformerAt(RouteLocation location)
    {
        return tests::ActorBuilder::sized(SmallBody).restingAt(location).walking();
    }

    // A small climber resting at the location, holding no surface yet.
    Actor climberAt(RouteLocation location)
    {
        return tests::ActorBuilder::sized(SmallBody).restingAt(location).walking().climbing(
            ClimbConfig);
    }

    glm::vec2 feetIn(Cell cell)
    {
        return feetInCell(tests::TileSize, cell);
    }

    Cell cellOf(glm::vec2 feet)
    {
        return cellAtFeet(tests::TileSize, feet);
    }

    // Where a small body's feet rest at the location.
    glm::vec2 feetAt(RouteLocation location)
    {
        return feetOf(boundsAtSurface(tests::TileSize, location, SmallBody));
    }

    // The search's result, or a test failure if the body rested nowhere.
    NavigationPathResult resultOf(const std::optional<NavigationPathResult>& result)
    {
        if (!result.has_value())
        {
            throw std::logic_error("The search found nowhere to start from");
        }
        return *result;
    }

    NavigationPath pathOf(const NavigationPathResult& result)
    {
        return result.path.value_or(NavigationPath{});
    }

    // A search for the actor after the fill has cached every cell of the map for it, as
    // the game's world does for each NPC. Nothing if the body rests nowhere.
    std::optional<NavigationPathResult> findActorPathAfterFill(
        const TileMap& map,
        const Actor& actor,
        glm::vec2 goalFeet,
        float stepSeconds = tests::FixedStepSeconds,
        FrameProfile* frameProfile = nullptr)
    {
        PlatformerConnectionCache cache;
        tests::fillConnections(map, cache, platformerTraversalProfileFor(actor, stepSeconds));
        return findActorPath(map, actor, goalFeet, stepSeconds, cache, frameProfile);
    }

    // The same, for an actor that rests somewhere: the result itself.
    NavigationPathResult findPath(
        const TileMap& map,
        const Actor& actor,
        glm::vec2 goalFeet,
        float stepSeconds = tests::FixedStepSeconds,
        FrameProfile* frameProfile = nullptr)
    {
        return resultOf(findActorPathAfterFill(map, actor, goalFeet, stepSeconds, frameProfile));
    }

    bool hasStep(const NavigationPath& path, Traversal traversal)
    {
        return std::any_of(
            path.waypoints.begin(),
            path.waypoints.end(),
            [traversal](const Waypoint& waypoint) { return waypoint.traversal == traversal; });
    }

    // Moves a climber along the path with the real follower and movement, as an NPC
    // moves, for up to tickLimit steps. Whether the follower got to the end. It starts
    // holding whatever surface its climb component holds.
    bool followsToTheEnd(
        const TileMap& map,
        const NavigationPath& path,
        Actor climber,
        int tickLimit)
    {
        if (!climber.platformerMovement.has_value() || !climber.surfaceClimb.has_value())
        {
            throw std::logic_error("Only a climber can follow a path here");
        }
        SurfaceClimb& climb = *climber.surfaceClimb;
        PlatformerMovement& movement = *climber.platformerMovement;
        movement.grounded = climb.surface == ClimbSurface::None;
        PathFollower follower;
        setPath(follower, path);
        for (int tick = 0; tick < tickLimit && !pathComplete(follower); ++tick)
        {
            const InputIntentions intentions = followPlatformerPath(
                climber.body, movement, follower, tests::FixedStepSeconds, &climb);
            updateSurfaceClimbMovement(
                map, climber.body, movement, climb, intentions, tests::FixedStepSeconds);
        }
        return pathComplete(follower);
    }

    glm::vec2 endOf(const NavigationPathResult& result)
    {
        return endOf(pathOf(result));
    }

    // A search with a cache the test sets up itself.
    std::optional<NavigationPathResult> searchWith(
        const TileMap& map,
        const Actor& actor,
        glm::vec2 goalFeet,
        PlatformerConnectionCache& cache,
        FrameProfile* frameProfile = nullptr)
    {
        return findActorPath(map, actor, goalFeet, tests::FixedStepSeconds, cache, frameProfile);
    }

    // Where navigation decides a search for this actor starts: the resting place it picked
    // from the body's position, which can differ from where the body's feet are. Nothing if
    // the body rests nowhere. The goal is the body's own feet only to keep the search short.
    std::optional<glm::vec2> startFeetOf(const TileMap& map, const Actor& actor)
    {
        const std::optional<NavigationPathResult> result =
            findActorPathAfterFill(map, actor, feetOf(actor.body.bounds));
        if (!result.has_value() || !result->path.has_value())
        {
            return std::nullopt;
        }
        return result->path->startFeet;
    }

    // A flyer needs no cache.
    std::optional<NavigationPathResult> findFlight(
        const TileMap& map,
        const Actor& flyer,
        glm::vec2 goalFeet,
        FrameProfile* frameProfile = nullptr)
    {
        PlatformerConnectionCache unused;
        return findActorPath(map, flyer, goalFeet, tests::FixedStepSeconds, unused, frameProfile);
    }

    TileMap climbableWall()
    {
        return tests::TileMapBuilder({"......", ".c....", ".c....", ".c....", ".c....", "######"})
            .where('c', tests::Tile{}.blocksMovement().climbable());
    }

    // Two floors either side of a block, joined only over its climbable walls and ceiling.
    TileMap climbOverBlock()
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
}

// Where a search starts

TEST_CASE("A platformer's path starts from the cell that holds it up", "[navigation][actor]")
{
    const TileMap map = tests::TileMapBuilder({"........", "........", "..###..."});

    const Actor standing = tests::ActorBuilder::sized(TallBody).inCell({3, 1}).walking();
    REQUIRE(startFeetOf(map, standing) == feetIn({3, 1}));
    // A body wider than a tile is still placed by its feet.
    const Actor wide = tests::ActorBuilder::sized({20.0F, 20.0F}).inCell({3, 1}).walking();
    REQUIRE(startFeetOf(map, wide) == feetIn({3, 1}));

    // At the ledge the feet hang past the platform, so the cell under them cannot be
    // stood on. The body starts from the cell that holds it up.
    const Actor atTheLedge = tests::ActorBuilder::sized(TallBody).atFeet({80.5F, 32.0F}).walking();
    REQUIRE(cellOf(feetOf(atTheLedge.body.bounds)) == Cell{5, 1});
    REQUIRE(startFeetOf(map, atTheLedge) == feetIn({4, 1}));

    // In the air, or a little above the floor, it rests nowhere and gets no result.
    const Actor inTheAir = tests::ActorBuilder::sized(TallBody).inCell({0, 0}).walking();
    REQUIRE_FALSE(startFeetOf(map, inTheAir));
    Actor hovering = tests::ActorBuilder::sized(TallBody).inCell({3, 1}).walking();
    hovering.body.bounds.topLeft.y -= 3.0F;
    REQUIRE_FALSE(startFeetOf(map, hovering));
}

TEST_CASE(
    "A climber's path starts from the surface its body is against",
    "[navigation][actor][climb]")
{
    const TileMap map = tests::TileMapBuilder({"cccccc", "c.....", "c.....", "c.....", "######"})
                            .where('c', tests::Tile{}.blocksMovement().climbable());

    // The climber is built holding no surface, so these starts come from where its body
    // rests alone, not from its climb component.

    // Partway up the wall, it starts from the nearest resting place along it.
    const RouteLocation wall{{1, 2}, ClimbSurface::LeftWall};
    Actor onWall = climberAt(wall);
    onWall.body.bounds.topLeft.y -= 5.0F;
    REQUIRE(startFeetOf(map, onWall) == feetAt(wall));
    // A platformer that cannot climb cannot hold the wall, so the same body rests nowhere.
    Actor platformerOnWall = platformerAt(wall);
    platformerOnWall.body.bounds.topLeft.y -= 5.0F;
    REQUIRE_FALSE(startFeetOf(map, platformerOnWall));

    const RouteLocation hanging{{3, 1}, ClimbSurface::Ceiling};
    REQUIRE(startFeetOf(map, climberAt(hanging)) == feetAt(hanging));
}

TEST_CASE("Actors that climb differently get different traversal profiles", "[navigation][actor]")
{
    // The cache keeps connections per profile, so an actor that climbs, or climbs at a
    // different speed, must not share the connections of one that cannot.
    Actor actor = tests::ActorBuilder::sized(SmallBody).inCell({0, 0}).walking();
    const auto withoutClimbing = platformerTraversalProfileFor(actor, tests::FixedStepSeconds);
    REQUIRE_FALSE(withoutClimbing.climb.has_value());

    actor.surfaceClimb = SurfaceClimb{{60.0F}};
    const auto climbing = platformerTraversalProfileFor(actor, tests::FixedStepSeconds);
    REQUIRE(climbing.climb.has_value());
    REQUIRE(climbing.climb.value_or(SurfaceClimbConfig{}).speed == 60.0F);
    REQUIRE_FALSE(climbing == withoutClimbing);

    actor.surfaceClimb = SurfaceClimb{{90.0F}};
    REQUIRE_FALSE(platformerTraversalProfileFor(actor, tests::FixedStepSeconds) == climbing);
}

// Flying

TEST_CASE("A flying path crosses open cells around a wall", "[navigation][flying]")
{
    const TileMap map = tests::TileMapBuilder({"....", ".##.", "...."});

    const Actor flyer = tests::ActorBuilder::sized(FlyerSize).inCell({0, 1}).flying(60.0F);
    FrameProfile profile;
    const NavigationPathResult result = resultOf(findFlight(map, flyer, feetIn({3, 1}), &profile));

    REQUIRE(result.status == NavigationPathStatus::Found);
    REQUIRE(result.path.has_value());
    const NavigationPath path = pathOf(result);
    REQUIRE(cellOf(path.startFeet) == Cell{0, 1});
    REQUIRE(cellOf(path.waypoints.back().feet) == Cell{3, 1});
    REQUIRE(path.waypoints.front().traversal == Traversal::Fly);
    // A flying search expands cells without running any movement.
    REQUIRE(frameStatisticCount(profile, "Cells expanded") >= 1);
}

TEST_CASE("A flyer whose feet are off the map gets no result", "[navigation][flying]")
{
    const TileMap map = tests::TileMapBuilder({"...", "..."});
    const Actor flyer = tests::ActorBuilder::sized(FlyerSize).inCell({3, 0}).flying(60.0F);
    REQUIRE_FALSE(findFlight(map, flyer, feetIn({0, 0})).has_value());
}

// Platformers

TEST_CASE(
    "The jump start penalty stops a needless hop but not a needed jump",
    "[navigation][platformer][regression]")
{
    // A hop over the platform along the way would save a few ticks, not enough to be
    // worth the penalty added to every jump.
    const TileMap hop =
        tests::TileMapBuilder({".....###.....", ".............", ".............", "#############"});
    PlatformerMovementConfig slow;
    slow.maximumSpeed = 60.0F;
    const Actor slowPlatformer = tests::ActorBuilder::sized(TallBody).inCell({12, 2}).walking(slow);
    const NavigationPath preferred = pathOf(findPath(hop, slowPlatformer, feetIn({4, 2})));
    REQUIRE_FALSE(preferred.waypoints.empty());
    REQUIRE_FALSE(hasStep(preferred, Traversal::Jump));

    // A goal on a platform is reached only by jumping, penalty or not.
    const TileMap platform =
        tests::TileMapBuilder({"..........", "....##....", "..........", "##########"});
    const Actor platformer = platformerAt({{2, 2}});
    const std::vector<RouteConnection> connections =
        buildPlatformerConnections(
            platform, {2, 2}, platformerTraversalProfileFor(platformer, tests::FixedStepSeconds))
            .connections;
    const RouteConnection& up = tests::jumpUpFrom(connections, 2);
    const NavigationPath climbed =
        pathOf(findPath(platform, platformer, feetIn(up.step.destination.cell)));
    REQUIRE(hasStep(climbed, Traversal::Jump));
}

TEST_CASE(
    "A platformer search rejects a step that is not finite and positive",
    "[navigation][platformer][validation]")
{
    const TileMap map = tests::TileMapBuilder({"...", "###"});
    const Actor platformer = platformerAt({{0, 0}});
    for (const float step :
         {0.0F, -tests::FixedStepSeconds, std::numeric_limits<float>::infinity()})
    {
        REQUIRE_THROWS_AS(findPath(map, platformer, feetIn({1, 0}), step), std::invalid_argument);
    }
}

TEST_CASE(
    "A goal out of reach reports how far the path's end is from it",
    "[navigation][platformer]")
{
    const TileMap map = tests::TileMapBuilder({"........", "........", "........", "########"});
    const glm::vec2 midJump = feetIn({5, 1}) - glm::vec2{0.0F, 4.0F};

    const NavigationPathResult result = findPath(map, platformerAt({{1, 2}}), midJump);

    // The goal is in the air, where no body can rest, so the path stops as close as it
    // can: on the floor below.
    REQUIRE(result.status == NavigationPathStatus::Unreachable);
    REQUIRE(result.path.has_value());
    REQUIRE(endOf(result) == feetAt({{5, 2}}));
    REQUIRE(result.remainingDistance > 0.0F);
}

// Climbing

TEST_CASE(
    "A path that starts partway along a wall can be followed to its end",
    "[navigation][platformer][climb]")
{
    const TileMap map = climbableWall();
    const RouteLocation start{{2, 3}, ClimbSurface::LeftWall};
    const NavigationPathResult result =
        findPath(map, climberAt(start), feetAt({{2, 2}, ClimbSurface::LeftWall}));
    REQUIRE(result.path.has_value());

    // The climber that follows it holds the wall a little above where the path starts.
    Actor climber = climberAt(start);
    climber.body.bounds.topLeft.y -= 6.0F;
    climber.surfaceClimb = SurfaceClimb{ClimbConfig, ClimbSurface::LeftWall};
    REQUIRE(followsToTheEnd(map, pathOf(result), climber, 120));
}

TEST_CASE(
    "One path crosses a floor, a wall, a ceiling and another floor",
    "[navigation][platformer][climb]")
{
    const TileMap map = climbOverBlock();
    const RouteLocation start{{2, 5}};

    const NavigationPathResult withoutClimbing =
        findPath(map, platformerAt(start), feetIn({11, 5}));
    REQUIRE(withoutClimbing.status == NavigationPathStatus::Unreachable);

    const NavigationPathResult result = findPath(map, climberAt(start), feetIn({11, 5}));
    REQUIRE(result.status == NavigationPathStatus::Found);
    REQUIRE(result.path.has_value());
    const NavigationPath path = pathOf(result);
    const auto passes = [&path](RouteLocation location)
    {
        return std::any_of(
            path.waypoints.begin(),
            path.waypoints.end(),
            [location](const Waypoint& waypoint) { return waypoint.feet == feetAt(location); });
    };
    REQUIRE(passes({{2, 3}, ClimbSurface::LeftWall}));
    REQUIRE(passes({{6, 2}, ClimbSurface::Ceiling}));
    REQUIRE(passes({{11, 3}, ClimbSurface::RightWall}));
    // Holding the wall at the foot of the far side already counts as the goal's cell.
    REQUIRE(cellAtFeet(tests::TileSize, endOf(result)) == Cell{11, 5});

    REQUIRE(followsToTheEnd(map, pathOf(result), climberAt(start), 2000));
}

// Searching with the connection cache

TEST_CASE("A search never simulates or writes to the cache", "[navigation][cache]")
{
    const TileMap map =
        tests::TileMapBuilder({"........", "........", "........", "###..###", "########"});
    const RouteLocation start{{0, 2}};
    const glm::vec2 goalFeet = feetIn({7, 2});
    const Actor platformer = platformerAt(start);
    // The cache keys what it holds by the platformer's profile.
    const PlatformerTraversalProfile profile =
        platformerTraversalProfileFor(platformer, tests::FixedStepSeconds);
    PlatformerConnectionCache cache;

    // With an empty cache, the search stores nothing. It puts the cell it needs at the
    // front of the fill, and the result is Deferred.
    FrameProfile waiting;
    const NavigationPathResult deferred =
        resultOf(searchWith(map, platformer, goalFeet, cache, &waiting));
    REQUIRE(deferred.status == NavigationPathStatus::Deferred);
    REQUIRE_FALSE(deferred.path.has_value());
    REQUIRE(cache.cachedCellCount(profile) == 0);
    REQUIRE(cache.cellsPending(profile) == 1);
    REQUIRE(cache.nextPending(profile) == start.cell);
    REQUIRE(frameStatisticCount(waiting, "Path searches") == 1);
    REQUIRE(frameStatisticCount(waiting, "Cells expanded") == 0);
    REQUIRE(frameStatisticCount(waiting, "Paths deferred") == 1);

    // Once the fill has cached the map, the search reads it and still writes nothing.
    tests::fillConnections(map, cache, profile);
    const std::size_t cachedBeforeSearching = cache.size();
    FrameProfile reading;
    const NavigationPathResult found =
        resultOf(searchWith(map, platformer, goalFeet, cache, &reading));
    REQUIRE(found.status == NavigationPathStatus::Found);
    REQUIRE(frameStatisticCount(reading, "Cells expanded") > 0);
    REQUIRE(cache.size() == cachedBeforeSearching);
    REQUIRE(cache.cellsPending(profile) == 0);
}

TEST_CASE(
    "A search waits for a cell the cache does not hold yet, even when a costlier path exists",
    "[navigation][cache]")
{
    // The start leads to the goal two ways: cheaply through a cell the cache does not
    // hold yet, or straight there at a cost of 100. The search waits for the missing cell
    // rather than take the costly way, and moves it ahead of an unrelated cell in the
    // fill. Once the cell is cached, the path goes through it.
    const TileMap map = tests::TileMapBuilder({"...."});
    const Cell start{0, 0};
    const Cell pending{1, 0};
    const Cell goal{2, 0};
    const Cell unrelated{3, 0};
    const Actor platformer = platformerAt({start});
    const PlatformerTraversalProfile profile =
        platformerTraversalProfileFor(platformer, tests::FixedStepSeconds);
    PlatformerConnectionCache cache;
    cache.storeConnections(
        start,
        profile,
        {{{{pending}, Traversal::Walk, {}}, 1}, {{{goal}, Traversal::Walk, {}}, 100}},
        {start, goal});
    cache.queue(unrelated, profile);
    cache.queue(pending, profile);
    REQUIRE(cache.nextPending(profile) == unrelated);

    FrameProfile waiting;
    const auto deferred = resultOf(searchWith(map, platformer, feetIn(goal), cache, &waiting));
    REQUIRE(deferred.status == NavigationPathStatus::Deferred);
    REQUIRE_FALSE(deferred.path.has_value());
    REQUIRE(frameStatisticCount(waiting, "Paths deferred") == 1);
    REQUIRE(frameStatisticCount(waiting, "Cells expanded") == 1);
    REQUIRE(cache.nextPending(profile) == pending);

    cache.storeConnections(pending, profile, {{{{goal}, Traversal::Walk, {}}, 1}}, {pending, goal});
    const auto found = resultOf(searchWith(map, platformer, feetIn(goal), cache));
    REQUIRE(found.status == NavigationPathStatus::Found);
    REQUIRE(found.path.has_value());
    const NavigationPath path = pathOf(found);
    REQUIRE(path.waypoints.size() == 2);
    REQUIRE(path.waypoints.front().feet == feetIn(pending));
}
