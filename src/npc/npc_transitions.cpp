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
    }

    std::optional<NpcState> nextNpcState(NpcState state, const NpcFacts& facts)
    {
        switch (state)
        {
        case NpcState::Idle:
            if (facts.targetKnown)
            {
                return NpcState::Chase;
            }
            if (facts.hasPatrol)
            {
                return NpcState::Patrol;
            }
            return std::nullopt;
        case NpcState::Patrol:
            if (facts.targetKnown)
            {
                return NpcState::Chase;
            }
            return std::nullopt;
        case NpcState::Chase:
            if (!facts.targetKnown)
            {
                return patrolOrIdle(facts);
            }
            if (facts.targetInBiteRange)
            {
                return NpcState::Bite;
            }
            return std::nullopt;
        case NpcState::Bite:
            // The bite starts the update after it is asked for, so a bite still ready on
            // the entering update has not begun; the wait lets the attack system see it.
            if (!facts.biteReady || facts.stateElapsed <= 0.0F)
            {
                return std::nullopt;
            }
            return facts.targetKnown ? NpcState::Chase : patrolOrIdle(facts);
        }
        return std::nullopt;
    }
}
