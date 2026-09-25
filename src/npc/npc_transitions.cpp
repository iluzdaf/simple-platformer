#include "simple_platformer/npc/npc_transitions.hpp"

#include <optional>

#include "simple_platformer/npc/npc.hpp"

namespace simple_platformer
{
    namespace
    {
        NpcState patrolOrIdle(const NpcFacts& facts)
        {
            return facts.hasPatrol ? NpcState::Patrol : NpcState::Idle;
        }

        // Where a lost target leaves its pursuer: searching for it, or back to its routine.
        NpcState lostTarget(const NpcFacts& facts)
        {
            return facts.searches ? NpcState::Search : patrolOrIdle(facts);
        }

        // The one choice the table puts to the tactic: how a known target is pursued.
        // With the attack that can reach it now, a bite before a shot, and otherwise by
        // chasing; a KeepDistance NPC first backs away from a target that has come too
        // near. Nothing without a target.
        std::optional<NpcState> pursuit(NpcTactic tactic, const NpcFacts& facts)
        {
            if (!facts.targetKnown)
            {
                return std::nullopt;
            }
            if (tactic == NpcTactic::KeepDistance && facts.targetTooClose)
            {
                return NpcState::Retreat;
            }
            if (facts.targetInBiteRange)
            {
                return NpcState::Bite;
            }
            if (facts.canShootTarget)
            {
                return NpcState::Shoot;
            }
            return NpcState::Chase;
        }
    }

    std::optional<NpcState> nextNpcState(NpcTactic tactic, NpcState state, const NpcFacts& facts)
    {
        const std::optional<NpcState> pursuing = pursuit(tactic, facts);
        switch (state)
        {
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
                return lostTarget(facts);
            }
            return *pursuing != state ? pursuing : std::nullopt;
        case NpcState::Search:
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
            return facts.targetKnown ? NpcState::Chase : lostTarget(facts);
        }
        return std::nullopt;
    }
}
