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

        // How a known target is pursued: with the attack that can reach it now, a bite
        // before a shot, and otherwise by chasing. Nothing without a target.
        std::optional<NpcState> pursuit(const NpcFacts& facts)
        {
            if (!facts.targetKnown)
            {
                return std::nullopt;
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

    std::optional<NpcState> nextNpcState(NpcState state, const NpcFacts& facts)
    {
        const std::optional<NpcState> pursuing = pursuit(facts);
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
