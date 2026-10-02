#pragma once

#include <optional>

#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_facts.hpp"

namespace simple_platformer
{
    // Returns the state to enter, or nothing to stay. Uses only the tactic, current state,
    // and facts; it does not change the actor or issue intentions.
    std::optional<NpcState> nextNpcState(NpcTactic tactic, NpcState state, const NpcFacts& facts);
}
