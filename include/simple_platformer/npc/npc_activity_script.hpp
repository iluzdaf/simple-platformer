#pragma once

#include <map>
#include <optional>
#include <string>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_transitions.hpp"

namespace simple_platformer
{
    // The engine-owned knowledge copied into one script update. Lua can change its copy,
    // but none of those changes reach the simulation.
    struct NpcActivitySnapshot
    {
        glm::vec2 feet = {0.0F, 0.0F};
        std::optional<glm::vec2> targetFeet;
        NpcFacts facts;
        bool pathComplete = false;
        std::map<std::string, float> tuning;
    };

    // What a script asks the engine to attempt. Applying the command remains C++ work.
    struct NpcActivityCommand
    {
        InputIntentions intentions;
        std::optional<glm::vec2> routeTo;
        std::optional<glm::vec2> aimAt;
        bool clearRoute = false;
    };

    // The core-facing boundary. Tests can provide a fake without loading Lua.
    class NpcActivityScripts
    {
    public:
        virtual ~NpcActivityScripts() = default;

        virtual void enter(
            ActorId actor,
            const LuaNpcActivity& activity,
            const NpcActivitySnapshot& snapshot) = 0;
        virtual NpcActivityCommand update(
            ActorId actor,
            const LuaNpcActivity& activity,
            const NpcActivitySnapshot& snapshot,
            float deltaTime) = 0;
        virtual void exit(
            ActorId actor,
            const LuaNpcActivity& activity,
            const NpcActivitySnapshot& snapshot) = 0;
        virtual void forget(ActorId actor) = 0;
    };
}
