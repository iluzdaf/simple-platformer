#include "simple_platformer/render/cover_fade.hpp"

#include <algorithm>
#include <optional>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/sight.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
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

    void updateCoverFades(const TileMap& map, World& world, float deltaTime)
    {
        const Actor* player = world.findActor(world.playerId());
        const std::optional<glm::vec2> viewer =
            player != nullptr ? std::optional(centerOf(player->body.bounds)) : std::nullopt;

        for (Actor& actor : world.actors())
        {
            if (actor.id == world.playerId())
            {
                continue;
            }
            fadeTowards(
                actor.screenVisibility,
                visibility(map, viewer, actor.body.bounds, ScreenCoverFade),
                deltaTime);
        }
        for (Pickup& pickup : world.pickups())
        {
            fadeTowards(
                pickup.screenVisibility,
                visibility(map, viewer, pickup.bounds, ScreenCoverFade),
                deltaTime);
        }
    }
}
