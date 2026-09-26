#pragma once

#include <string>

#include "simple_platformer/npc/npc_activity.hpp"

namespace simple_platformer
{
    enum class NpcTactic;

    // The names the overlay prints for an NPC's state and tactic.
    const char* nameOf(NpcState state);
    const char* nameOf(NpcTactic tactic);
    std::string nameOf(const NpcActivity& activity);
}
