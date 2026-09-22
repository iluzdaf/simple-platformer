#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;
    struct Aabb;
    struct Actor;
    struct NpcSenses;

    bool canSeeTarget(
        const TileMap& map,
        const Aabb& observer,
        const Aabb& target,
        const NpcSenses& senses);
    // Whether any other living actor with senses can see the target. Teams and brains are
    // not considered: this asks what the world could see, not who is hunting whom.
    bool seenByAnyNpc(const TileMap& map, const World& world, const Actor& target);
    void updateNpcSenses(const TileMap& map, World& world, float deltaTime);
}
