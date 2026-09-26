#include "npc_names.hpp"

#include <string>
#include <variant>

#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_activity.hpp"

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

    std::string nameOf(const NpcActivity& activity)
    {
        if (const auto* builtIn = std::get_if<BuiltInNpcActivity>(&activity))
        {
            return "builtin: " + std::string(nameOf(builtIn->state));
        }
        const LuaNpcActivity& scripted = std::get<LuaNpcActivity>(activity);
        return "lua: " + scripted.script + "." + scripted.activity;
    }
}
