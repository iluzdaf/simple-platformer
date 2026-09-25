#pragma once

#include "simple_platformer/npc/npc_transitions.hpp"

namespace tests
{
    // Builds the facts a transition decides on, stated in the order a test reads them:
    //
    //   NpcFactsBuilder::facts().withPatrol().knowingTarget().biteReadyFor(0.1F)
    //
    // Every fact starts false, so a chain names only the ones the transition depends on.
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

        // A visible target inside the bite's hitbox, which is known as well.
        NpcFactsBuilder targetInBiteRange() &&
        {
            built.targetKnown = true;
            built.targetVisible = true;
            built.targetInBiteRange = true;
            return *this;
        }

        // A visible target with a ranged weapon to shoot it, which is known as well.
        NpcFactsBuilder canShootTarget() &&
        {
            built.targetKnown = true;
            built.targetVisible = true;
            built.canShootTarget = true;
            return *this;
        }

        // A known target nearer than the standoff distance.
        NpcFactsBuilder targetTooClose() &&
        {
            built.targetKnown = true;
            built.targetTooClose = true;
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
