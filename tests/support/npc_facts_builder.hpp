#pragma once

#include "simple_platformer/npc/npc_facts.hpp"

namespace tests
{
    // Builds the facts a transition decides on, stated in the order a test reads them:
    //
    //   NpcFactsBuilder::facts().withPatrol().knowingTarget().biteReadyFor(0.1F)
    //
    // Flags start false and elapsed times start at zero. A chain sets only the facts
    // the transition depends on.
    // The chain converts to NpcFacts wherever one is expected, such as nextNpcState.
    class NpcFactsBuilder
    {
    public:
        static NpcFactsBuilder facts()
        {
            return {};
        }

        NpcFactsBuilder withPatrol() &&
        {
            built.hasPatrol = true;
            return *this;
        }

        NpcFactsBuilder knowingTarget() &&
        {
            built.targetKnown = true;
            return *this;
        }

        // Sets the target as known, visible, and inside the bite's hitbox.
        NpcFactsBuilder targetInBiteRange() &&
        {
            built.targetKnown = true;
            built.targetVisible = true;
            built.targetInBiteRange = true;
            return *this;
        }

        // Sets a known, visible target and the fact that the NPC can shoot it.
        NpcFactsBuilder targetInSights() &&
        {
            built.targetKnown = true;
            built.targetVisible = true;
            built.targetInSights = true;
            return *this;
        }

        // A known target nearer than the standoff distance.
        NpcFactsBuilder targetWithinStandoffDistance() &&
        {
            built.targetKnown = true;
            built.targetWithinStandoffDistance = true;
            return *this;
        }

        // The NPC searches for a lost target, with time still to search.
        NpcFactsBuilder searching() &&
        {
            built.searches = true;
            return *this;
        }

        // The NPC searches for a lost target, and its search has run its time.
        NpcFactsBuilder searchTimeUp() &&
        {
            built.searches = true;
            built.searchTimeUp = true;
            return *this;
        }

        // The bite is ready this many seconds into the state.
        NpcFactsBuilder biteReadyFor(float stateElapsed) &&
        {
            built.biteReady = true;
            built.stateElapsed = stateElapsed;
            return *this;
        }

        operator simple_platformer::NpcFacts() &&
        {
            return built;
        }

    private:
        simple_platformer::NpcFacts built;
    };
}
