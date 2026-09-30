#include "simple_platformer/npc/npc_facts.hpp"

#include <glm/geometric.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_senses.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    namespace
    {
        bool targetIsInBiteRange(const Actor& actor, const Actor& target)
        {
            if (!actor.bite.has_value())
            {
                return false;
            }
            return overlaps(
                biteHitbox(actor.body.bounds, *actor.bite, actor.facing), target.body.bounds);
        }

        bool targetIsWithinStandoffDistance(const Actor& actor, const NpcBrain& brain)
        {
            const float standoffDistance =
                actor.senses.has_value() ? actor.senses->standoffDistance : 0.0F;
            return glm::distance(feetOf(actor.body.bounds), brain.lastKnownTargetFeet) <
                   standoffDistance;
        }

        bool targetIsOnSameRun(const TileMap& map, const Actor& actor, const Actor& target)
        {
            return actor.platformerMovement.has_value() && actor.platformerMovement->grounded &&
                   target.platformerMovement.has_value() && target.platformerMovement->grounded &&
                   onSameGroundRun(map, actor.body.bounds, target.body.bounds);
        }

        bool targetIsWithinNoticeDistance(const Actor& actor, const Actor& target)
        {
            return actor.senses.has_value() &&
                   glm::distance(feetOf(actor.body.bounds), feetOf(target.body.bounds)) <=
                       actor.senses->noticeDistance;
        }
    }

    NpcFacts gatherNpcFacts(
        const TileMap& map,
        const Actor& actor,
        const NpcBrain& brain,
        const NpcPerception& perception,
        const Actor* target,
        float stateElapsed)
    {
        NpcFacts facts;
        facts.targetKnown = target != nullptr;
        facts.targetVisible = perception.targetVisible;
        facts.targetInBiteRange =
            target != nullptr && perception.targetVisible && targetIsInBiteRange(actor, *target);
        facts.biteReady = actor.bite.has_value() && actor.bite->phase == BitePhase::Ready;
        facts.targetInSights =
            target != nullptr && perception.targetVisible && actor.rangedWeapon.has_value();
        facts.targetWithinStandoffDistance =
            target != nullptr && targetIsWithinStandoffDistance(actor, brain);
        facts.heardLanding = perception.heardLanding;
        facts.targetOnSameRun = target != nullptr && targetIsOnSameRun(map, actor, *target);
        facts.targetWithinNoticeDistance =
            target != nullptr && targetIsWithinNoticeDistance(actor, *target);
        facts.movementBlocked =
            actor.platformerMovement.has_value() && actor.platformerMovement->blocked;
        facts.hasPatrol = actor.patrol.has_value();
        const float searchDuration = actor.senses.has_value() ? actor.senses->searchDuration : 0.0F;
        facts.searches = searchDuration > 0.0F;
        facts.searchTimeUp = stateElapsed >= searchDuration;
        facts.stateElapsed = stateElapsed;
        return facts;
    }
}
