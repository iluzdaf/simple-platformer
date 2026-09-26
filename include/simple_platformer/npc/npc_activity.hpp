#pragma once

#include <string>
#include <variant>

#include "simple_platformer/npc/npc.hpp"

namespace simple_platformer
{
    struct BuiltInNpcActivity
    {
        NpcState state = NpcState::Idle;

        bool operator==(const BuiltInNpcActivity& other) const
        {
            return state == other.state;
        }
    };

    struct LuaNpcActivity
    {
        std::string script;
        std::string activity;

        bool operator==(const LuaNpcActivity& other) const
        {
            return script == other.script && activity == other.activity;
        }
    };

    using NpcActivity = std::variant<BuiltInNpcActivity, LuaNpcActivity>;
}
