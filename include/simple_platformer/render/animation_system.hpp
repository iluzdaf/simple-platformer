#pragma once

namespace simple_platformer
{
    class World;

    // Advances presentation animation for world objects. This is intentionally separate
    // from the gameplay-only world simulation.
    void updateWorldAnimations(World& world, float deltaTime);
}
