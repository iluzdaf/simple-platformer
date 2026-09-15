#pragma once

#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"

namespace simple_platformer
{
    class World;
    class WorldRequests;

    Aabb biteHitbox(const Aabb& actorBounds, const BiteAttack& bite, Facing facing);
    void updateAttacks(World& world, WorldRequests& requests, float deltaTime);
}
