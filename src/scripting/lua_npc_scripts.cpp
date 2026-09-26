#include "simple_platformer/scripting/lua_npc_scripts.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

// sol2 supports its public API through this umbrella header. Listing its internal headers
// would couple the adapter to implementation details without improving include hygiene.
// NOLINTBEGIN(misc-include-cleaner)
#include <lua.hpp>
#include <sol/sol.hpp>

#include "simple_platformer/math/validation.hpp"

namespace simple_platformer
{
    namespace
    {
        constexpr int InstructionsPerCall = 100'000;

        void instructionBudgetExceeded(lua_State* state, lua_Debug*)
        {
            luaL_error(state, "instruction budget exceeded");
        }

        class InstructionBudget
        {
        public:
            explicit InstructionBudget(lua_State* state)
                : state(state)
            {
                lua_sethook(state, instructionBudgetExceeded, LUA_MASKCOUNT, InstructionsPerCall);
            }

            ~InstructionBudget()
            {
                lua_sethook(state, nullptr, 0, 0);
            }

            InstructionBudget(const InstructionBudget&) = delete;
            InstructionBudget& operator=(const InstructionBudget&) = delete;

        private:
            lua_State* state;
        };

        struct ActivityOwner
        {
            std::uint32_t actor = 0;
            std::string script;
            std::string activity;

            bool operator<(const ActivityOwner& other) const
            {
                return std::tie(actor, script, activity) <
                       std::tie(other.actor, other.script, other.activity);
            }
        };

        struct LoadedScript
        {
            std::string source;
            sol::environment environment;
            sol::table activities;
        };

        template <std::size_t Size>
        bool contains(const std::array<std::string_view, Size>& values, std::string_view wanted)
        {
            for (std::string_view value : values)
            {
                if (value == wanted)
                {
                    return true;
                }
            }
            return false;
        }

        template <std::size_t Size>
        void rejectUnknownFields(
            const sol::table& table,
            const std::array<std::string_view, Size>& allowed,
            std::string_view subject)
        {
            for (const auto& [keyObject, value] : table)
            {
                (void)value;
                if (!keyObject.is<std::string>())
                {
                    throw std::invalid_argument(
                        std::string(subject) + " has a field whose name is not text");
                }
                const std::string key = keyObject.as<std::string>();
                if (!contains(allowed, key))
                {
                    throw std::invalid_argument(
                        std::string(subject) + " has unknown field '" + key + "'");
                }
            }
        }

        float number(const sol::object& object, std::string_view field)
        {
            if (object.get_type() != sol::type::number)
            {
                throw std::invalid_argument(std::string(field) + " must be a number");
            }
            const double value = object.as<double>();
            if (!std::isfinite(value) ||
                value < -static_cast<double>(std::numeric_limits<float>::max()) ||
                value > static_cast<double>(std::numeric_limits<float>::max()))
            {
                throw std::invalid_argument(std::string(field) + " must be finite");
            }
            return static_cast<float>(value);
        }

        bool boolean(const sol::object& object, std::string_view field)
        {
            if (object.get_type() != sol::type::boolean)
            {
                throw std::invalid_argument(std::string(field) + " must be true or false");
            }
            return object.as<bool>();
        }

        glm::vec2 vector(const sol::object& object, std::string_view field)
        {
            if (!object.is<sol::table>())
            {
                throw std::invalid_argument(std::string(field) + " must be an {x, y} table");
            }
            const sol::table table = object.as<sol::table>();
            rejectUnknownFields(table, std::array<std::string_view, 2>{"x", "y"}, field);
            return {
                number(table.get<sol::object>("x"), std::string(field) + ".x"),
                number(table.get<sol::object>("y"), std::string(field) + ".y")};
        }

        void requireValidCall(ActorId actor, const LuaNpcActivity& activity)
        {
            if (actor.value == 0)
            {
                throw std::invalid_argument("NPC script calls require a valid actor ID");
            }
            if (activity.script.empty() || activity.activity.empty())
            {
                throw std::invalid_argument("NPC script calls require a script and activity name");
            }
        }

        sol::table luaVector(sol::state& lua, glm::vec2 value)
        {
            return lua.create_table_with("x", value.x, "y", value.y);
        }

        sol::table luaSnapshot(sol::state& lua, const NpcActivitySnapshot& snapshot)
        {
            sol::table result = lua.create_table();
            result["feet"] = luaVector(lua, snapshot.feet);
            result["targetFeet"] = snapshot.targetFeet.has_value()
                                       ? sol::make_object(lua, luaVector(lua, *snapshot.targetFeet))
                                       : sol::make_object(lua, sol::lua_nil);
            result["stateElapsed"] = snapshot.facts.stateElapsed;
            result["pathComplete"] = snapshot.pathComplete;

            sol::table facts = lua.create_table();
            facts["targetKnown"] = snapshot.facts.targetKnown;
            facts["targetVisible"] = snapshot.facts.targetVisible;
            facts["targetInBiteRange"] = snapshot.facts.targetInBiteRange;
            facts["biteReady"] = snapshot.facts.biteReady;
            facts["targetInSights"] = snapshot.facts.targetInSights;
            facts["targetTooClose"] = snapshot.facts.targetTooClose;
            facts["hasPatrol"] = snapshot.facts.hasPatrol;
            facts["searches"] = snapshot.facts.searches;
            facts["searchTimeUp"] = snapshot.facts.searchTimeUp;
            result["facts"] = facts;

            sol::table tuning = lua.create_table();
            for (const auto& [name, value] : snapshot.tuning)
            {
                tuning[name] = value;
            }
            result["tuning"] = tuning;
            return result;
        }

        NpcActivityCommand commandFrom(const sol::object& object)
        {
            NpcActivityCommand command;
            if (!object.valid() || object.get_type() == sol::type::lua_nil)
            {
                return command;
            }
            if (!object.is<sol::table>())
            {
                throw std::invalid_argument(
                    "an activity update must return a command table or nil");
            }

            const sol::table table = object.as<sol::table>();
            constexpr std::array<std::string_view, 8> Fields{
                "direction",
                "aimDirection",
                "jumpPressed",
                "jumpHeld",
                "primaryAttackPressed",
                "routeTo",
                "aimAt",
                "clearRoute"};
            rejectUnknownFields(table, Fields, "an activity command");

            const auto readVector = [&](std::string_view name, glm::vec2& destination)
            {
                const sol::object value = table.get<sol::object>(name);
                if (value.valid() && value.get_type() != sol::type::lua_nil)
                {
                    destination = vector(value, std::string("command.") + std::string(name));
                }
            };
            const auto readOptionalVector =
                [&](std::string_view name, std::optional<glm::vec2>& destination)
            {
                const sol::object value = table.get<sol::object>(name);
                if (value.valid() && value.get_type() != sol::type::lua_nil)
                {
                    destination = vector(value, std::string("command.") + std::string(name));
                }
            };
            const auto readBoolean = [&](std::string_view name, bool& destination)
            {
                const sol::object value = table.get<sol::object>(name);
                if (value.valid() && value.get_type() != sol::type::lua_nil)
                {
                    destination = boolean(value, std::string("command.") + std::string(name));
                }
            };

            readVector("direction", command.intentions.direction);
            readVector("aimDirection", command.intentions.aimDirection);
            readBoolean("jumpPressed", command.intentions.jumpPressed);
            readBoolean("jumpHeld", command.intentions.jumpHeld);
            readBoolean("primaryAttackPressed", command.intentions.primaryAttackPressed);
            readOptionalVector("routeTo", command.routeTo);
            readOptionalVector("aimAt", command.aimAt);
            readBoolean("clearRoute", command.clearRoute);
            return command;
        }

        std::string resultError(sol::protected_function_result& result)
        {
            const sol::error error = result;
            return error.what();
        }

        std::string scriptDescription(std::string_view script, std::string_view source)
        {
            std::string description = "Lua script '";
            description.append(script);
            description.append("' in ");
            description.append(source);
            return description;
        }

        std::string activityDescription(
            std::string_view script,
            std::string_view activity,
            std::string_view source)
        {
            std::string description = "Lua activity '";
            description.append(script);
            description.push_back('.');
            description.append(activity);
            description.append("' in ");
            description.append(source);
            return description;
        }

        [[noreturn]] void fail(std::string description, std::string_view reason)
        {
            description.append(reason);
            throw std::invalid_argument(description);
        }
    }

    struct LuaNpcScripts::Implementation
    {
        sol::state lua;
        std::map<std::string, LoadedScript> scripts;
        std::map<ActivityOwner, sol::table> selves;
        std::vector<LuaScriptDiagnostic> reported;

        Implementation()
        {
            lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
            lua["dofile"] = sol::lua_nil;
            lua["load"] = sol::lua_nil;
            lua["loadfile"] = sol::lua_nil;
            lua["require"] = sol::lua_nil;
        }

        LoadedScript* scriptNamed(std::string_view name)
        {
            const auto found = scripts.find(std::string(name));
            return found == scripts.end() ? nullptr : &found->second;
        }

        const LoadedScript* scriptNamed(std::string_view name) const
        {
            const auto found = scripts.find(std::string(name));
            return found == scripts.end() ? nullptr : &found->second;
        }

        std::optional<sol::table> activityTable(const LuaNpcActivity& activity) const
        {
            const LoadedScript* script = scriptNamed(activity.script);
            if (script == nullptr)
            {
                return std::nullopt;
            }
            const sol::object found = script->activities.get<sol::object>(activity.activity);
            if (!found.is<sol::table>())
            {
                return std::nullopt;
            }
            return found.as<sol::table>();
        }

        void report(
            ActorId actor,
            const LuaNpcActivity& activity,
            std::string hook,
            std::string message)
        {
            const LoadedScript* script = scriptNamed(activity.script);
            reported.push_back(
                {script == nullptr ? std::string{} : script->source,
                 activity.script,
                 activity.activity,
                 std::move(hook),
                 actor,
                 std::move(message)});
        }

        void discardScriptState(std::string_view script)
        {
            for (auto entry = selves.begin(); entry != selves.end();)
            {
                if (entry->first.script == script)
                {
                    entry = selves.erase(entry);
                }
                else
                {
                    ++entry;
                }
            }
        }
    };

    LuaNpcScripts::LuaNpcScripts()
        : implementation(std::make_unique<Implementation>())
    {
    }

    LuaNpcScripts::~LuaNpcScripts() = default;
    LuaNpcScripts::LuaNpcScripts(LuaNpcScripts&& other) noexcept = default;
    LuaNpcScripts& LuaNpcScripts::operator=(LuaNpcScripts&& other) noexcept = default;

    void LuaNpcScripts::loadScript(const std::string& script, const std::filesystem::path& path)
    {
        std::ifstream input(path);
        if (!input)
        {
            throw std::invalid_argument("Cannot read Lua script '" + path.string() + "'");
        }
        std::ostringstream source;
        source << input.rdbuf();
        loadScriptText(script, source.str(), path.string());
    }

    void LuaNpcScripts::loadScriptText(
        const std::string& script,
        std::string_view source,
        std::string sourceName)
    {
        if (script.empty())
        {
            throw std::invalid_argument("A Lua script needs a name");
        }
        if (sourceName.empty())
        {
            throw std::invalid_argument("A Lua script needs a source name");
        }

        sol::environment fresh(implementation->lua, sol::create, implementation->lua.globals());
        sol::protected_function_result result;
        {
            const InstructionBudget budget(implementation->lua.lua_state());
            result = implementation->lua.safe_script(
                source, fresh, sol::script_pass_on_error, sourceName);
        }
        if (!result.valid())
        {
            const std::string error = resultError(result);
            std::string reason = ": ";
            reason.append(error);
            fail(scriptDescription(script, sourceName), reason);
        }

        const sol::object returned = result;
        if (!returned.is<sol::table>())
        {
            fail(scriptDescription(script, sourceName), " must return a table");
        }
        const sol::table root = returned.as<sol::table>();
        rejectUnknownFields(root, std::array<std::string_view, 1>{"activities"}, "a Lua script");
        const sol::object activitiesObject = root.get<sol::object>("activities");
        if (!activitiesObject.is<sol::table>())
        {
            fail(scriptDescription(script, sourceName), " needs an activities table");
        }

        const sol::table activities = activitiesObject.as<sol::table>();
        std::set<std::string> names;
        for (const auto& [nameObject, activityObject] : activities)
        {
            if (!nameObject.is<std::string>())
            {
                fail(
                    scriptDescription(script, sourceName),
                    " has an activity whose name is not text");
            }
            const std::string name = nameObject.as<std::string>();
            if (name.empty() || !activityObject.is<sol::table>())
            {
                fail(scriptDescription(script, sourceName), " needs named activity tables");
            }
            names.insert(name);
            const sol::table activity = activityObject.as<sol::table>();
            rejectUnknownFields(
                activity,
                std::array<std::string_view, 3>{"enter", "update", "exit"},
                "Lua activity '" + name + "'");
            if (!activity.get<sol::object>("update").is<sol::function>())
            {
                fail(activityDescription(script, name, sourceName), " needs an update function");
            }
            for (std::string_view optional : {std::string_view{"enter"}, std::string_view{"exit"}})
            {
                const sol::object hook = activity.get<sol::object>(optional);
                if (hook.valid() && hook.get_type() != sol::type::lua_nil &&
                    !hook.is<sol::function>())
                {
                    std::string reason = " has a ";
                    reason.append(optional);
                    reason.append(" value that is not a function");
                    fail(activityDescription(script, name, sourceName), reason);
                }
            }
        }
        if (names.empty())
        {
            fail(scriptDescription(script, sourceName), " needs at least one activity");
        }

        implementation->discardScriptState(script);
        implementation->scripts.insert_or_assign(
            script, LoadedScript{std::move(sourceName), std::move(fresh), activities});
    }

    bool LuaNpcScripts::hasScript(std::string_view script) const
    {
        return implementation->scriptNamed(script) != nullptr;
    }

    bool LuaNpcScripts::hasActivity(const LuaNpcActivity& activity) const
    {
        return implementation->activityTable(activity).has_value();
    }

    void LuaNpcScripts::enter(
        ActorId actor,
        const LuaNpcActivity& activity,
        const NpcActivitySnapshot& snapshot)
    {
        requireValidCall(actor, activity);
        const std::optional<sol::table> table = implementation->activityTable(activity);
        if (!table.has_value())
        {
            implementation->report(actor, activity, "enter", "activity is not loaded");
            return;
        }

        const ActivityOwner owner{actor.value, activity.script, activity.activity};
        sol::table self = implementation->lua.create_table();
        implementation->selves.insert_or_assign(owner, self);
        const sol::object hookObject = table->get<sol::object>("enter");
        if (!hookObject.valid() || hookObject.get_type() == sol::type::lua_nil)
        {
            return;
        }

        sol::protected_function hook = hookObject.as<sol::protected_function>();
        sol::protected_function_result result;
        {
            const InstructionBudget budget(implementation->lua.lua_state());
            result = hook(self, luaSnapshot(implementation->lua, snapshot));
        }
        if (!result.valid())
        {
            implementation->report(actor, activity, "enter", resultError(result));
        }
    }

    NpcActivityCommand LuaNpcScripts::update(
        ActorId actor,
        const LuaNpcActivity& activity,
        const NpcActivitySnapshot& snapshot,
        float deltaTime)
    {
        requireValidCall(actor, activity);
        requireSeconds(deltaTime, "NPC script update time step");

        const std::optional<sol::table> table = implementation->activityTable(activity);
        if (!table.has_value())
        {
            implementation->report(actor, activity, "update", "activity is not loaded");
            return {};
        }
        const ActivityOwner owner{actor.value, activity.script, activity.activity};
        const auto self = implementation->selves.find(owner);
        if (self == implementation->selves.end())
        {
            implementation->report(actor, activity, "update", "activity was not entered");
            return {};
        }

        const sol::protected_function hook =
            table->get<sol::object>("update").as<sol::protected_function>();
        sol::protected_function_result result;
        {
            const InstructionBudget budget(implementation->lua.lua_state());
            result = hook(self->second, luaSnapshot(implementation->lua, snapshot), deltaTime);
        }
        if (!result.valid())
        {
            implementation->report(actor, activity, "update", resultError(result));
            return {};
        }

        try
        {
            const sol::object returned = result;
            return commandFrom(returned);
        }
        catch (const std::invalid_argument& error)
        {
            implementation->report(actor, activity, "update", error.what());
            return {};
        }
    }

    void LuaNpcScripts::exit(
        ActorId actor,
        const LuaNpcActivity& activity,
        const NpcActivitySnapshot& snapshot)
    {
        requireValidCall(actor, activity);
        const ActivityOwner owner{actor.value, activity.script, activity.activity};
        const auto self = implementation->selves.find(owner);
        if (self == implementation->selves.end())
        {
            return;
        }

        const std::optional<sol::table> table = implementation->activityTable(activity);
        if (table.has_value())
        {
            const sol::object hookObject = table->get<sol::object>("exit");
            if (hookObject.valid() && hookObject.get_type() != sol::type::lua_nil)
            {
                sol::protected_function hook = hookObject.as<sol::protected_function>();
                sol::protected_function_result result;
                {
                    const InstructionBudget budget(implementation->lua.lua_state());
                    result = hook(self->second, luaSnapshot(implementation->lua, snapshot));
                }
                if (!result.valid())
                {
                    implementation->report(actor, activity, "exit", resultError(result));
                }
            }
        }
        implementation->selves.erase(self);
    }

    void LuaNpcScripts::forget(ActorId actor)
    {
        for (auto entry = implementation->selves.begin(); entry != implementation->selves.end();)
        {
            if (entry->first.actor == actor.value)
            {
                entry = implementation->selves.erase(entry);
            }
            else
            {
                ++entry;
            }
        }
    }

    const std::vector<LuaScriptDiagnostic>& LuaNpcScripts::diagnostics() const
    {
        return implementation->reported;
    }

    void LuaNpcScripts::clearDiagnostics()
    {
        implementation->reported.clear();
    }
}

// NOLINTEND(misc-include-cleaner)
