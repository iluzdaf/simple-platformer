#pragma once

#include <optional>
#include <string>
#include "animation_catalog.hpp"
#include "machine_catalog.hpp"
#include <glm/vec2.hpp>
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/npc/npc.hpp"

namespace simple_platformer
{
    // Initial component settings; each composition creates fresh runtime state.
    struct ActorDefinition
    {
        // Content declares it; composition rejects a size left at zero.
        glm::vec2 bodySize = {0.0F, 0.0F};
        Team team = Team::Neutral;
        Facing facing = Facing::Right;
        SpriteAnchor spriteAnchor = SpriteAnchor::BodyFeet;
        std::string animations;
        std::optional<int> health;
        std::optional<int> inventorySlots;
        std::optional<PlatformerMovementConfig> platformer;
        std::optional<FlyingMovement> flying;
        // Presence enables the existing NPC brain and path follower together.
        std::optional<NpcSenses> senses;
        // The brain's policy.
        NpcTactic tactic = NpcTactic::Pursuer;
        // A data-driven machine in the machine catalog, run instead of the tactic. Empty
        // for none.
        std::string machine;
        // Reuse the engine's attack settings. Composition resets their phase/timer state;
        // JSON exposes only configuration fields, never those runtime fields.
        std::optional<BiteAttack> bite;
        std::optional<RangedWeapon> ranged;
    };

    Actor composeActor(
        const ActorDefinition& definition,
        const AnimationCatalog& animations,
        int textureId,
        glm::vec2 spawnFeet = {},
        std::optional<Patrol> patrol = std::nullopt,
        const MachineCatalog& machines = {});
    void validateActorDefinition(
        const ActorDefinition& definition,
        const AnimationCatalog& animations,
        const MachineCatalog& machines = {});
}
