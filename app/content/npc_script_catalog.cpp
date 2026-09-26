#include "npc_script_catalog.hpp"

#include "machine_catalog.hpp"

#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <variant>

#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"
#include "simple_platformer/scripting/lua_npc_scripts.hpp"

namespace simple_platformer
{
    namespace
    {
        void requireFileStem(const std::string& script)
        {
            const std::filesystem::path name(script);
            if (script == "." || script == ".." || name.has_root_path() || name.has_parent_path() ||
                name.filename() != name || name.has_extension())
            {
                throw std::invalid_argument(
                    "Lua NPC script name '" + script + "' must be a file stem, not a path");
            }
        }
    }

    void loadNpcActivityScripts(
        LuaNpcScripts& scripts,
        const MachineCatalog& machines,
        const std::filesystem::path& directory)
    {
        std::set<std::string> loaded;
        for (const auto& entry : machines)
        {
            const NpcStateMachine& machine = entry.second;
            for (const NpcMachineState& state : machine.states)
            {
                const auto* activity = std::get_if<LuaNpcActivity>(&state.does);
                if (activity == nullptr || !loaded.insert(activity->script).second)
                {
                    continue;
                }
                requireFileStem(activity->script);
                scripts.loadScript(activity->script, directory / (activity->script + ".lua"));
            }
        }

        for (const auto& [machineName, machine] : machines)
        {
            for (const NpcMachineState& state : machine.states)
            {
                const auto* activity = std::get_if<LuaNpcActivity>(&state.does);
                if (activity != nullptr && !scripts.hasActivity(*activity))
                {
                    throw std::invalid_argument(
                        "State '" + state.name + "' in machine '" + machineName +
                        "' references unknown Lua activity '" + activity->script + "." +
                        activity->activity + "'");
                }
            }
        }
    }
}
