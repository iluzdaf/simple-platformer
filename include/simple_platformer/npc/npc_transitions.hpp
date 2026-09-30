#pragma once

#include <optional>

#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_facts.hpp"

namespace simple_platformer
{
    // The state to enter from this one given the facts, or nothing to stay. Every
    // transition the NPC makes is a branch here, and none of them act. The tactic is
    // asked wherever the table makes a choice: the pursuit of a known target, and where
    // a lost one leaves the NPC. Every other transition is shared.
    std::optional<NpcState> nextNpcState(NpcTactic tactic, NpcState state, const NpcFacts& facts);
}
