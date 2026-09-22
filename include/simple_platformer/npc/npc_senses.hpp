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
    // Whether any NPC's senses reached the player this update. Reads what updateNpcSenses
    // stamped on each brain, so gameplay and the screen agree on who is seen.
    bool playerSeenByAnyNpc(const World& world);
    void updateNpcSenses(const TileMap& map, World& world, float deltaTime);
}
