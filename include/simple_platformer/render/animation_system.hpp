#pragma once

namespace simple_platformer
{
    class World;

    // Selects and advances the animation of every animated actor. This is presentation
    // work and is intentionally separate from the gameplay-only world simulation.
    void updateActorAnimations(World& world, float deltaTime);
}
