#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;
    struct Aabb;
    struct NpcSenses;

    bool canSeeTarget(
        const TileMap& map,
        const Aabb& observer,
        const Aabb& target,
        const NpcSenses& senses);
    void updateNpcSenses(const TileMap& map, World& world, float deltaTime);
}
