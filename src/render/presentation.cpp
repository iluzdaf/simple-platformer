#include "simple_platformer/render/presentation.hpp"

#include "simple_platformer/render/animation_system.hpp"
#include "simple_platformer/render/cover_fade.hpp"

namespace simple_platformer
{
    void updateWorldPresentation(const TileMap& map, World& world, float deltaTime)
    {
        updateWorldAnimations(world, deltaTime);
        updateCoverFades(map, world, deltaTime);
    }
}
