#pragma once

namespace simple_platformer
{
    class TileMap;
    class World;
    struct Aabb;
    struct Actor;
    struct NpcBrain;
    struct NpcSenses;

    // The actor the brain remembers, while it is still alive; otherwise nothing.
    const Actor* livingTarget(const World& world, const NpcBrain& brain);

    bool canSeeTarget(
        const TileMap& map,
        const Aabb& observer,
        const Aabb& target,
        const NpcSenses& senses);
    // Checks for a supported walk run beneath both feet; does not check grounded state.
    bool onSameGroundRun(const TileMap& map, const Aabb& observer, const Aabb& target);
    // Reads the latest sensing result used by gameplay and cover presentation.
    bool playerSeenByAnyNpc(const World& world);
    void updateNpcSenses(const TileMap& map, World& world, float deltaTime);
}
