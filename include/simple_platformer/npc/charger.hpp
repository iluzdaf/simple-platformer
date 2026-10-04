#pragma once

#include <optional>

#include "simple_platformer/npc/npc.hpp"

namespace simple_platformer
{
    struct Actor;
    struct NpcFacts;
    struct NpcUpdate;

    // Charger normally chooses Sleep, Charge, or Stunned.
    std::optional<NpcState> nextChargerState(NpcState state, const NpcFacts& facts);
    void enterChargerState(Actor& actor, NpcBrain& brain, NpcState state);
    void updateChargerState(
        const NpcUpdate& update,
        Actor& actor,
        const NpcBrain& brain,
        const Actor* target,
        NpcState state);
}
