#pragma once

#include <string_view>
#include <vector>

#include "simple_platformer/npc/npc_transitions.hpp"

namespace simple_platformer
{
    // A fact a data-driven transition asks for by name, answered from NpcFacts. The words
    // are what the overlay and a load-time message say about the condition.
    struct NpcFactRow
    {
        std::string_view name;
        const char* whenTrue;
        const char* whenFalse;
        bool (*holds)(const NpcFacts& facts);
    };

    // Every fact by name, in the order the overlay lists them. The enum brain's own
    // bookkeeping, whether it searches at all and how long it has been in its state, has
    // no row: a data-driven transition holds for a time instead.
    const std::vector<NpcFactRow>& npcFactRows();
    // The row of that name, or nothing.
    const NpcFactRow* npcFactRow(std::string_view name);
}
