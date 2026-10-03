#include "simple_platformer/npc/charger.hpp"

#include <optional>

#include <glm/geometric.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_facts.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/npc/npc_behaviour.hpp"
#include "simple_platformer/npc/npc_update.hpp"

namespace simple_platformer
{
    namespace
    {
        constexpr float RecoverySeconds = 1.5F;

        bool canFaceTargetWhileStunned(const TileMap& map, const Actor& actor, const Actor* target)
        {
            if (target == nullptr)
            {
                return false;
            }
            return actor.platformerMovement.has_value() && actor.platformerMovement->grounded &&
                   target->platformerMovement.has_value() && target->platformerMovement->grounded &&
                   onSameGroundRun(map, actor.body.bounds, target->body.bounds) &&
                   actor.senses.has_value() &&
                   glm::distance(feetOf(actor.body.bounds), feetOf(target->body.bounds)) <=
                       actor.senses->noticeDistance;
        }
    }

    std::optional<NpcState> nextChargerState(NpcState state, const NpcFacts& facts)
    {
        const bool canCharge = facts.targetOnSameRun && facts.targetWithinNoticeDistance;
        switch (state)
        {
        case NpcState::Charge:
            return facts.movementBlocked ? std::optional(NpcState::Stunned) : std::nullopt;
        case NpcState::Stunned:
            if (facts.stateElapsed < RecoverySeconds)
            {
                return std::nullopt;
            }
            return canCharge ? NpcState::Charge : NpcState::Sleep;
        default:
            if (facts.heardLanding && canCharge)
            {
                return NpcState::Charge;
            }
            return state == NpcState::Sleep ? std::nullopt : std::optional(NpcState::Sleep);
        }
    }

    void enterChargerState(Actor& actor, NpcBrain& brain, NpcState state)
    {
        if (state == NpcState::Charge)
        {
            brain.chargeDirection =
                brain.lastKnownTargetFeet.x < feetOf(actor.body.bounds).x ? -1.0F : 1.0F;
        }
    }

    void updateChargerState(
        const NpcUpdate& update,
        Actor& actor,
        const NpcBrain& brain,
        const Actor* target,
        NpcState state)
    {
        switch (state)
        {
        case NpcState::Sleep:
            break;
        case NpcState::Charge:
            actor.intentions.direction = {brain.chargeDirection, 0.0F};
            actor.intentions.avoidLedges = true;
            actor.intentions.contactDamage = true;
            break;
        case NpcState::Stunned:
            if (canFaceTargetWhileStunned(update.map, actor, target))
            {
                aimToward(actor, brain.lastKnownTargetFeet);
            }
            break;
        default:
            break;
        }
    }
}
