#pragma once

#include <stdexcept>
#include <string>
#include <utility>

#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"

namespace tests
{
    // Builds a data-driven machine in the words the json uses, stated in the order the
    // machine lists them:
    //
    //   NpcMachineBuilder::named("test")
    //       .state("rest", NpcState::Idle)
    //       .state("hunt", NpcState::Chase)
    //       .transition("rest", "hunt").when("targetKnown", true)
    //       .transition("hunt", "rest").when("targetKnown", false).after(0.5F)
    //
    // when() and after() describe the transition added last. The builder holds no rules of
    // its own: startNpcMachine validates what it built. It converts to an NpcStateMachine
    // wherever one is expected, such as the actor builder's running().
    class NpcMachineBuilder
    {
    public:
        static NpcMachineBuilder named(std::string name)
        {
            NpcMachineBuilder builder;
            builder.built.name = std::move(name);
            return builder;
        }

        NpcMachineBuilder state(std::string name, simple_platformer::NpcState does) &&
        {
            built.states.push_back({std::move(name), does});
            return std::move(*this);
        }

        NpcMachineBuilder transition(std::string from, std::string to) &&
        {
            simple_platformer::NpcMachineTransition transition;
            transition.from = std::move(from);
            transition.to = std::move(to);
            built.transitions.push_back(std::move(transition));
            return std::move(*this);
        }

        NpcMachineBuilder when(const std::string& fact, bool holds) &&
        {
            lastTransition().when[fact] = holds;
            return std::move(*this);
        }

        NpcMachineBuilder after(float seconds) &&
        {
            lastTransition().after = seconds;
            return std::move(*this);
        }

        operator simple_platformer::NpcStateMachine() &&
        {
            return std::move(built);
        }

    private:
        simple_platformer::NpcMachineTransition& lastTransition()
        {
            if (built.transitions.empty())
            {
                throw std::logic_error("when() and after() describe a transition added first");
            }
            return built.transitions.back();
        }

        simple_platformer::NpcStateMachine built;
    };
}
