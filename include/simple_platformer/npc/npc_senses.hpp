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
    // Whether any living NPC's senses reach the target, by the same rule they use to
    // notice the player. Teams are not considered.
    bool seenByAnyNpc(const TileMap& map, const World& world, const Actor& target);
    void updateNpcSenses(const TileMap& map, World& world, float deltaTime);
}
