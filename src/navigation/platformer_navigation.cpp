#include "simple_platformer/navigation/platformer_navigation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/input_program.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/timing/fixed_step.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace
{
    constexpr float SimulationStep = static_cast<float>(simple_platformer::FixedDeltaSeconds);
    constexpr int MaximumSimulationTicks = 120;

    bool sameIntentions(
        const simple_platformer::InputIntentions& first,
        const simple_platformer::InputIntentions& second)
    {
        return first.direction == second.direction && first.jumpPressed == second.jumpPressed &&
               first.jumpHeld == second.jumpHeld &&
               first.primaryAttackPressed == second.primaryAttackPressed;
    }

    void recordSimulationInput(
        simple_platformer::InputProgram& program,
        const simple_platformer::InputIntentions& intentions)
    {
        if (!program.empty() && sameIntentions(program.back().intentions, intentions))
        {
            program.back().duration += SimulationStep;
            return;
        }
        program.push_back({SimulationStep, intentions});
    }

    simple_platformer::Aabb bodyAt(simple_platformer::GridPosition position, glm::vec2 bodySize)
    {
        simple_platformer::Aabb bounds{{0.0F, 0.0F}, bodySize};
        simple_platformer::placeFeetAt(bounds, simple_platformer::navigationFeet(position));
        return bounds;
    }

    bool bodyFits(const simple_platformer::TileMap& map, const simple_platformer::Aabb& bounds)
    {
        constexpr float Inside = 0.001F;
        const float tileSize = static_cast<float>(simple_platformer::TileSize);
        const int firstColumn =
            static_cast<int>(std::floor((bounds.position.x + Inside) / tileSize));
        const int lastColumn =
            static_cast<int>(std::floor((bounds.position.x + bounds.size.x - Inside) / tileSize));
        const int firstRow = static_cast<int>(std::floor((bounds.position.y + Inside) / tileSize));
        const int lastRow =
            static_cast<int>(std::floor((bounds.position.y + bounds.size.y - Inside) / tileSize));

        for (int row = firstRow; row <= lastRow; ++row)
        {
            for (int column = firstColumn; column <= lastColumn; ++column)
            {
                if (map.blocksMovement({column, row}))
                {
                    return false;
                }
            }
        }
        return true;
    }

    int gridDistance(simple_platformer::GridPosition first, simple_platformer::GridPosition second)
    {
        return std::abs(first.x - second.x) + std::abs(first.y - second.y);
    }

    std::optional<simple_platformer::NavigationNeighbor> simulateTraversal(
        const simple_platformer::TileMap& map,
        simple_platformer::GridPosition start,
        glm::vec2 bodySize,
        const simple_platformer::PlatformerMovementConfig& config,
        simple_platformer::Traversal traversal,
        float direction,
        int jumpHoldTicks)
    {
        simple_platformer::Body body{bodyAt(start, bodySize), {0.0F, 0.0F}};
        simple_platformer::PlatformerMovement movement{config, true, 0.0F, 0.0F};
        simple_platformer::Facing facing =
            direction < 0.0F ? simple_platformer::Facing::Left : simple_platformer::Facing::Right;
        simple_platformer::InputProgram program;
        bool leftGround = false;
        std::optional<simple_platformer::GridPosition> landing;

        for (int tick = 0; tick < MaximumSimulationTicks; ++tick)
        {
            constexpr float WallTolerance = 0.001F;
            if ((direction < 0.0F && body.bounds.position.x <= WallTolerance) ||
                (direction > 0.0F &&
                 body.bounds.position.x + body.bounds.size.x >= map.pixelWidth() - WallTolerance))
            {
                return std::nullopt;
            }

            simple_platformer::InputIntentions intentions;
            intentions.direction.x = landing.has_value() ? 0.0F : direction;
            if (traversal == simple_platformer::Traversal::Jump && !landing.has_value())
            {
                intentions.jumpPressed = tick == 0;
                intentions.jumpHeld = tick < jumpHoldTicks;
            }
            recordSimulationInput(program, intentions);
            simple_platformer::updatePlatformerMovement(
                map, body, movement, intentions, facing, SimulationStep);

            leftGround = leftGround || !movement.grounded;
            if (!leftGround || !movement.grounded)
            {
                continue;
            }

            const simple_platformer::GridPosition destination =
                simple_platformer::navigationCell(simple_platformer::feetOf(body.bounds));
            if (!landing.has_value())
            {
                if (destination == start ||
                    !simple_platformer::canStandAt(map, destination, bodySize))
                {
                    return std::nullopt;
                }
                landing = destination;
            }
            if (body.velocity.x != 0.0F)
            {
                continue;
            }
            if (destination != landing.value_or(start))
            {
                return std::nullopt;
            }
            const int ticks = tick + 1;
            return simple_platformer::NavigationNeighbor{
                destination, traversal, std::max(ticks, gridDistance(start, destination)), program};
        }
        return std::nullopt;
    }

    void keepCheapest(
        std::vector<simple_platformer::NavigationNeighbor>& neighbors,
        simple_platformer::NavigationNeighbor candidate)
    {
        const auto existing = std::find_if(
            neighbors.begin(),
            neighbors.end(),
            [&candidate](const simple_platformer::NavigationNeighbor& neighbor)
            {
                return neighbor.destination == candidate.destination &&
                       neighbor.traversal == candidate.traversal;
            });
        if (existing == neighbors.end())
        {
            neighbors.push_back(std::move(candidate));
        }
        else if (candidate.cost < existing->cost)
        {
            *existing = std::move(candidate);
        }
    }
}

namespace simple_platformer
{
    bool canStandAt(const TileMap& map, GridPosition position, glm::vec2 bodySize)
    {
        if (!isFinite(bodySize) || bodySize.x <= 0.0F || bodySize.y <= 0.0F)
        {
            throw std::invalid_argument("Navigation body size must be finite and positive");
        }
        return map.contains(position) && !map.isSolid(position) &&
               map.blocksMovement({position.x, position.y + 1}) &&
               bodyFits(map, bodyAt(position, bodySize));
    }

    std::vector<NavigationNeighbor> platformerNeighbors(
        const TileMap& map,
        GridPosition position,
        glm::vec2 bodySize,
        const PlatformerMovementConfig& movement)
    {
        if (!canStandAt(map, position, bodySize))
        {
            return {};
        }

        std::vector<NavigationNeighbor> neighbors;
        constexpr std::array<int, 2> Directions{-1, 1};
        for (const int direction : Directions)
        {
            const GridPosition adjacent{position.x + direction, position.y};
            if (canStandAt(map, adjacent, bodySize))
            {
                neighbors.push_back({adjacent, Traversal::Walk, 1, {}});
            }
            else
            {
                const std::optional<NavigationNeighbor> fall = simulateTraversal(
                    map,
                    position,
                    bodySize,
                    movement,
                    Traversal::Fall,
                    static_cast<float>(direction),
                    0);
                if (fall.has_value())
                {
                    keepCheapest(neighbors, fall.value());
                }
            }

            constexpr std::array<int, 2> JumpHoldTicks{1, MaximumSimulationTicks};
            for (const int holdTicks : JumpHoldTicks)
            {
                const std::optional<NavigationNeighbor> jump = simulateTraversal(
                    map,
                    position,
                    bodySize,
                    movement,
                    Traversal::Jump,
                    static_cast<float>(direction),
                    holdTicks);
                if (jump.has_value())
                {
                    keepCheapest(neighbors, jump.value());
                }
            }
        }
        return neighbors;
    }
}
