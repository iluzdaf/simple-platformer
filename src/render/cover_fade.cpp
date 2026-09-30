#include "simple_platformer/render/cover_fade.hpp"

#include <algorithm>
#include <optional>

#include <glm/common.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/sight.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        bool firedWithinRevealWindow(const World& world, const Actor& player)
        {
            if (!player.rangedWeapon.has_value())
            {
                return false;
            }
            const std::optional<float> sinceShot =
                world.secondsSince(player.rangedWeapon->lastFiredTimeSeconds);
            return sinceShot.has_value() && *sinceShot < ShotRevealSeconds;
        }

        float playerTarget(const TileMap& map, const World& world, const Actor& player)
        {
            if (playerSeenByAnyNpc(world) || firedWithinRevealWindow(world, player))
            {
                return 1.0F;
            }
            return visibility(map, std::nullopt, player.body.bounds, ScreenCoverFade);
        }

        void fadeTowards(std::optional<float>& shown, float target, float deltaTime)
        {
            if (!shown.has_value())
            {
                shown = target;
                return;
            }
            const float mostChange = deltaTime / CoverFadeSeconds;
            *shown += std::clamp(target - *shown, -mostChange, mostChange);
        }
    }

    float fractionInCover(const TileMap& map, const Aabb& bounds)
    {
        const float area = bounds.size.x * bounds.size.y;
        if (area <= 0.0F)
        {
            return 0.0F;
        }

        const glm::vec2 boundsEnd = bounds.topLeft + bounds.size;
        const Cell first = cellAt(map.tileSize(), bounds.topLeft);
        const Cell last = cellAt(map.tileSize(), boundsEnd);
        float coveredArea = 0.0F;
        for (int row = first.y; row <= last.y; ++row)
        {
            for (int column = first.x; column <= last.x; ++column)
            {
                if (!map.blocksSight({column, row}))
                {
                    continue;
                }
                const glm::vec2 cellStart = cellCorner(map.tileSize(), {column, row});
                const glm::vec2 cellEnd = cellStart + static_cast<float>(map.tileSize());
                const glm::vec2 overlap = glm::max(
                    glm::min(boundsEnd, cellEnd) - glm::max(bounds.topLeft, cellStart),
                    glm::vec2{0.0F, 0.0F});
                coveredArea += overlap.x * overlap.y;
            }
        }
        return std::min(coveredArea / area, 1.0F);
    }

    float visibility(
        const TileMap& map,
        std::optional<glm::vec2> viewer,
        const Aabb& target,
        CoverFade fade)
    {
        const float covered = fractionInCover(map, target);
        if (covered <= fade.concealsAbove)
        {
            return 1.0F;
        }
        if (viewer.has_value() && lineOfSight(map, *viewer, centerOf(target)))
        {
            return 1.0F;
        }
        return std::clamp(
            (fade.hidesAbove - covered) / (fade.hidesAbove - fade.concealsAbove), 0.0F, 1.0F);
    }

    void updateCoverFades(const TileMap& map, World& world, float deltaTime)
    {
        requireSeconds(deltaTime, "Cover fades time step");
        const Actor* player = world.findActor(world.playerId());
        const std::optional<glm::vec2> viewer =
            player != nullptr ? std::optional(centerOf(player->body.bounds)) : std::nullopt;

        for (Actor& actor : world.actors())
        {
            fadeTowards(
                actor.screenVisibility,
                actor.id == world.playerId()
                    ? playerTarget(map, world, actor)
                    : visibility(map, viewer, actor.body.bounds, ScreenCoverFade),
                deltaTime);
        }
        for (Pickup& pickup : world.pickups())
        {
            fadeTowards(
                pickup.screenVisibility,
                visibility(map, viewer, pickup.body.bounds, ScreenCoverFade),
                deltaTime);
        }
    }
}
