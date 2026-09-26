#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_transitions.hpp"

namespace simple_platformer
{
    // A state of a data-driven machine: its name in the data, and the activity the NPC
    // system runs while it is active.
    struct NpcMachineState
    {
        std::string name;
        NpcActivity does = BuiltInNpcActivity{};
    };

    // A transition fires once every fact in `when` has held for `after` seconds. The
    // facts are the rows in npc_fact_rows, by name.
    struct NpcMachineTransition
    {
        std::string from;
        std::string to;
        std::map<std::string, bool> when;
        float after = 0.0F;
    };

    // The machine as content declares it. The first state is the one it starts in, and
    // among the transitions from one state the first whose conditions hold wins.
    struct NpcStateMachine
    {
        std::string name;
        std::vector<NpcMachineState> states;
        std::vector<NpcMachineTransition> transitions;
    };

    // Rejects an empty machine, an empty or repeated state name, a transition from or to
    // a state the machine lacks, a condition naming a fact no row answers, and a hold
    // that is not a finite, non-negative time. The message names the transition.
    void validateNpcStateMachine(const NpcStateMachine& machine);
    std::size_t npcMachineStateNamed(const NpcStateMachine& machine, std::string_view name);

    // Whether every condition holds for these facts. A fact no row answers is an error.
    bool npcConditionsHold(const std::map<std::string, bool>& when, const NpcFacts& facts);

    // A running machine: an NPC component beside its brain. The brain keeps sensed-world
    // memory; the machine owns its active activity's lifecycle and elapsed time, along
    // with how long each transition's conditions have held.
    struct NpcMachine
    {
        NpcStateMachine definition;
        std::size_t active = 0;
        std::vector<float> heldFor;
        float stateElapsed = 0.0F;
        bool activityEntered = false;
        // The transition that fired last, for the overlay.
        std::optional<std::size_t> lastFired;
    };

    // A machine in its first state, validated.
    NpcMachine startNpcMachine(NpcStateMachine definition);
    // Advances the holds of the active state's transitions by the step and fires the
    // first whose conditions have held long enough, returning it; the machine is then in
    // that transition's state. At most one transition fires an update.
    std::optional<std::size_t> advanceNpcMachine(
        NpcMachine& machine,
        const NpcFacts& facts,
        float deltaTime);
    const NpcMachineState& activeNpcMachineState(const NpcMachine& machine);
}
