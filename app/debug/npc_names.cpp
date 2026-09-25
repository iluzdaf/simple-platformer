#include "npc_names.hpp"

#include "simple_platformer/npc/npc.hpp"

namespace simple_platformer
{
    const char* nameOf(NpcState state)
    {
        switch (state)
        {
        case NpcState::Idle:
            return "Idle";
        case NpcState::Patrol:
            return "Patrol";
        case NpcState::Chase:
            return "Chase";
        case NpcState::Bite:
            return "Bite";
        case NpcState::Shoot:
            return "Shoot";
        case NpcState::Search:
            return "Search";
        case NpcState::Retreat:
            return "Retreat";
        case NpcState::Watch:
            return "Watch";
        }

        return "Unknown";
    }

    const char* nameOf(NpcTactic tactic)
    {
        switch (tactic)
        {
        case NpcTactic::Pursuer:
            return "Pursuer";
        case NpcTactic::KeepDistance:
            return "KeepDistance";
        }

        return "Unknown";
    }
}
