#include "simple_platformer/npc/npc_transitions.hpp"
#include "simple_platformer/npc/npc_facts.hpp"

#include <optional>

#include "simple_platformer/npc/npc.hpp"

namespace simple_platformer
{
    namespace
    {
        constexpr float RecoverySeconds = 1.5F;

        NpcState patrolOrIdle(const NpcFacts& facts)
        {
            return facts.hasPatrol ? NpcState::Patrol : NpcState::Idle;
        }

        std::optional<NpcState> cowardState(NpcState state, const NpcFacts& facts)
        {
            switch (state)
            {
            case NpcState::Flee:
                if (facts.targetInBiteRange)
                {
                    return NpcState::Bite;
                }
                if (!facts.targetKnown && facts.targetLostElapsed >= RecoverySeconds)
                {
                    return patrolOrIdle(facts);
                }
                return std::nullopt;
            case NpcState::Bite:
                if (!facts.biteReady || facts.stateElapsed <= 0.0F)
                {
                    return std::nullopt;
                }
                return facts.targetKnown ? NpcState::Flee : patrolOrIdle(facts);
            default:
                if (facts.targetVisible && facts.targetWithinStandoffDistance)
                {
                    return NpcState::Flee;
                }
                if (state != patrolOrIdle(facts))
                {
                    return patrolOrIdle(facts);
                }
                return std::nullopt;
            }
        }

        std::optional<NpcState> chargerState(NpcState state, const NpcFacts& facts)
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

        // The second choice the table puts to the tactic: where a lost target leaves the
        // NPC. A Pursuer searches where it lost it, a KeepDistance NPC watches from where
        // it stands, and one that does not search goes back to its routine.
        NpcState lostTarget(NpcTactic tactic, const NpcFacts& facts)
        {
            if (!facts.searches)
            {
                return patrolOrIdle(facts);
            }
            return tactic == NpcTactic::KeepDistance ? NpcState::Watch : NpcState::Search;
        }

        // The first choice the table puts to the tactic: how a known target is pursued.
        // With the attack that can reach it now, a bite before a shot, and otherwise by
        // chasing; a KeepDistance NPC first backs away from a target that has come too
        // near. Nothing without a target.
        std::optional<NpcState> pursuit(NpcTactic tactic, const NpcFacts& facts)
        {
            if (!facts.targetKnown)
            {
                return std::nullopt;
            }
            if (tactic == NpcTactic::KeepDistance && facts.targetWithinStandoffDistance)
            {
                return NpcState::Retreat;
            }
            if (facts.targetInBiteRange)
            {
                return NpcState::Bite;
            }
            if (facts.targetInSights)
            {
                return NpcState::Shoot;
            }
            return NpcState::Chase;
        }
    }

    std::optional<NpcState> nextNpcState(NpcTactic tactic, NpcState state, const NpcFacts& facts)
    {
        switch (tactic)
        {
        case NpcTactic::Coward:
            return cowardState(state, facts);
        case NpcTactic::Charger:
            return chargerState(state, facts);
        case NpcTactic::Pursuer:
        case NpcTactic::KeepDistance:
            break;
        }
        const std::optional<NpcState> pursuing = pursuit(tactic, facts);
        switch (state)
        {
        case NpcState::Flee:
        case NpcState::Sleep:
        case NpcState::Charge:
        case NpcState::Stunned:
        case NpcState::Idle:
            if (pursuing.has_value())
            {
                return pursuing;
            }
            if (facts.hasPatrol)
            {
                return NpcState::Patrol;
            }
            return std::nullopt;
        case NpcState::Patrol:
            return pursuing;
        case NpcState::Chase:
        case NpcState::Shoot:
        case NpcState::Retreat:
            if (!pursuing.has_value())
            {
                return lostTarget(tactic, facts);
            }
            return *pursuing != state ? pursuing : std::nullopt;
        case NpcState::Search:
        case NpcState::Watch:
            if (pursuing.has_value())
            {
                return pursuing;
            }
            return facts.searchTimeUp ? std::optional(patrolOrIdle(facts)) : std::nullopt;
        case NpcState::Bite:
            // The bite starts the update after it is asked for, so a bite still ready on
            // the entering update has not begun; the wait lets the attack system see it.
            // A finished bite chases before it bites again.
            if (!facts.biteReady || facts.stateElapsed <= 0.0F)
            {
                return std::nullopt;
            }
            return facts.targetKnown ? NpcState::Chase : lostTarget(tactic, facts);
        }
        return std::nullopt;
    }
}
