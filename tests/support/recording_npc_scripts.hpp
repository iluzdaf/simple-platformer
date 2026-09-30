#pragma once

#include <string>
#include <vector>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_activity_scripts.hpp"

namespace tests
{
    struct ScriptCall
    {
        std::string hook;
        simple_platformer::ActorId actor;
        simple_platformer::LuaNpcActivity activity;
        simple_platformer::NpcActivitySnapshot snapshot;
    };

    // Stands in for the Lua runtime: records every hook it is called with, and answers
    // each update with the same command.
    class RecordingNpcScripts final : public simple_platformer::NpcActivityScripts
    {
    public:
        void enter(
            simple_platformer::ActorId actor,
            const simple_platformer::LuaNpcActivity& activity,
            const simple_platformer::NpcActivitySnapshot& snapshot) override
        {
            calls.push_back({"enter", actor, activity, snapshot});
        }

        simple_platformer::NpcActivityCommand update(
            simple_platformer::ActorId actor,
            const simple_platformer::LuaNpcActivity& activity,
            const simple_platformer::NpcActivitySnapshot& snapshot,
            float deltaTime) override
        {
            updateSteps.push_back(deltaTime);
            calls.push_back({"update", actor, activity, snapshot});
            return command;
        }

        void exit(
            simple_platformer::ActorId actor,
            const simple_platformer::LuaNpcActivity& activity,
            const simple_platformer::NpcActivitySnapshot& snapshot) override
        {
            calls.push_back({"exit", actor, activity, snapshot});
        }

        void forget(simple_platformer::ActorId actor) override
        {
            forgotten.push_back(actor);
        }

        simple_platformer::NpcActivityCommand command;
        std::vector<ScriptCall> calls;
        std::vector<float> updateSteps;
        std::vector<simple_platformer::ActorId> forgotten;
    };
}
