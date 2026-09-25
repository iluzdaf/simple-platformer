#include "machine_catalog.hpp"
#include "content_diagnostics.hpp"
#include "content_json.hpp"
#include <array>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <nlohmann/json.hpp>
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"

namespace simple_platformer
{
    namespace
    {
        using Json = nlohmann::json;

        struct ActivityEntry
        {
            std::string_view name;
            NpcState does;
        };

        // Every built-in activity a state may run, under the name the machine file uses.
        constexpr std::array<ActivityEntry, 8> Activities = {
            {{"idle", NpcState::Idle},
             {"patrol", NpcState::Patrol},
             {"chase", NpcState::Chase},
             {"bite", NpcState::Bite},
             {"shoot", NpcState::Shoot},
             {"search", NpcState::Search},
             {"retreat", NpcState::Retreat},
             {"watch", NpcState::Watch}}};

        NpcState jsonActivity(
            const Json& value,
            std::string_view sourceName,
            const std::string& path)
        {
            const std::string name = jsonText(value, sourceName, path);
            for (const ActivityEntry& entry : Activities)
            {
                if (entry.name == name)
                {
                    return entry.does;
                }
            }
            std::string expected;
            for (const ActivityEntry& entry : Activities)
            {
                expected += (expected.empty() ? "" : ", ") + std::string(entry.name);
            }
            failJson(
                sourceName, path, "unknown activity '" + name + "'; expected one of " + expected);
        }

        // A transition's `from` is one state name or a list of them; a list becomes one
        // transition per name, in order.
        std::vector<std::string> jsonSources(
            const Json& value,
            std::string_view sourceName,
            const std::string& path)
        {
            std::vector<std::string> sources;
            if (value.is_array())
            {
                for (std::size_t index = 0; index < value.size(); ++index)
                {
                    sources.push_back(jsonText(value[index], sourceName, indexPath(path, index)));
                }
                if (sources.empty())
                {
                    failJson(sourceName, path, "expected at least one state name");
                }
                return sources;
            }
            sources.push_back(jsonText(value, sourceName, path));
            return sources;
        }

        NpcStateMachine jsonMachine(
            const Json& value,
            const std::string& name,
            std::string_view sourceName,
            const std::string& path)
        {
            checkJsonFields(value, {"states", "transitions"}, sourceName, path);
            NpcStateMachine machine;
            machine.name = name;

            const std::string statesPath = fieldPath(path, "states");
            const Json& states = requiredJsonMember(value, "states", sourceName, path);
            if (!states.is_array())
            {
                failJson(sourceName, statesPath, "expected an array");
            }
            for (std::size_t index = 0; index < states.size(); ++index)
            {
                const std::string statePath = indexPath(statesPath, index);
                checkJsonFields(states[index], {"name", "does"}, sourceName, statePath);
                NpcMachineState state;
                state.name = readText(states[index], "name", sourceName, statePath);
                state.does = jsonActivity(
                    requiredJsonMember(states[index], "does", sourceName, statePath),
                    sourceName,
                    fieldPath(statePath, "does"));
                machine.states.push_back(state);
            }

            const std::string transitionsPath = fieldPath(path, "transitions");
            const Json& transitions = requiredJsonMember(value, "transitions", sourceName, path);
            if (!transitions.is_array())
            {
                failJson(sourceName, transitionsPath, "expected an array");
            }
            for (std::size_t index = 0; index < transitions.size(); ++index)
            {
                const std::string transitionPath = indexPath(transitionsPath, index);
                const Json& entry = transitions[index];
                checkJsonFields(entry, {"from", "to", "when", "after"}, sourceName, transitionPath);
                NpcMachineTransition transition;
                transition.to = readText(entry, "to", sourceName, transitionPath);
                const std::string whenPath = fieldPath(transitionPath, "when");
                const Json& when = requiredJsonMember(entry, "when", sourceName, transitionPath);
                checkJsonObject(when, sourceName, whenPath);
                for (const auto& condition : when.items())
                {
                    transition.when[condition.key()] = jsonBoolean(
                        condition.value(), sourceName, fieldPath(whenPath, condition.key()));
                }
                readOptionalNumber(entry, "after", transition.after, sourceName, transitionPath);
                const std::vector<std::string> sources = jsonSources(
                    requiredJsonMember(entry, "from", sourceName, transitionPath),
                    sourceName,
                    fieldPath(transitionPath, "from"));
                for (const std::string& from : sources)
                {
                    transition.from = from;
                    machine.transitions.push_back(transition);
                }
            }
            return machine;
        }
    }

    void validateMachineCatalog(const MachineCatalog& catalog)
    {
        for (const auto& entry : catalog)
        {
            try
            {
                if (entry.first.empty())
                {
                    throw std::invalid_argument("machine name cannot be empty");
                }
                validateNpcStateMachine(entry.second);
            }
            catch (const std::invalid_argument& error)
            {
                failJson({}, fieldPath("machines", entry.first), error.what());
            }
        }
    }

    MachineCatalog parseMachineCatalog(std::string_view text, std::string_view sourceName)
    {
        const auto root = parseContentRoot(text, sourceName);
        checkJsonFields(root, {"machines"}, sourceName, "root");
        const auto& definitions = requiredJsonMember(root, "machines", sourceName, "root");
        checkJsonObject(definitions, sourceName, "machines");
        MachineCatalog catalog;
        for (const auto& entry : definitions.items())
        {
            catalog.emplace(
                entry.key(),
                jsonMachine(
                    entry.value(), entry.key(), sourceName, fieldPath("machines", entry.key())));
        }
        validateInFile(sourceName, [&] { validateMachineCatalog(catalog); });
        return catalog;
    }

    MachineCatalog loadMachineCatalog(const std::filesystem::path& path)
    {
        return parseMachineCatalog(loadContentText(path), path.string());
    }

    const NpcStateMachine& npcStateMachine(const MachineCatalog& catalog, const std::string& name)
    {
        const auto found = catalog.find(name);
        if (found == catalog.end())
        {
            throw std::invalid_argument("unknown state machine '" + name + "'");
        }
        return found->second;
    }
}
