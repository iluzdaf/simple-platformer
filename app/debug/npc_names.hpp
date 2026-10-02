#pragma once

namespace simple_platformer
{
    enum class NpcState;
    enum class NpcTactic;

    // The names the overlay prints for an NPC's state and tactic.
    const char* nameOf(NpcState state);
    const char* nameOf(NpcTactic tactic);
}
