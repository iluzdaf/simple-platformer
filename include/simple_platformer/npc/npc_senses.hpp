#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;
    struct Aabb;
    struct Actor;
    struct NpcBrain;

    // The actor the brain remembers, while it is still alive; otherwise nothing.
    const Actor* livingTarget(const World& world, const NpcBrain& brain);

    // Checks for a supported walk run beneath both feet; does not check grounded state.
    bool onSameGroundRun(const TileMap& map, const Aabb& observer, const Aabb& target);
    void updateNpcSenses(const TileMap& map, World& world, float deltaTime);
}
