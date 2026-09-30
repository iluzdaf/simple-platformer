#include "lua_activity_values.hpp"

#include <cmath>
#include <initializer_list>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <glm/vec2.hpp>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/npc/npc_activity_scripts.hpp"

// sol2 supports its public API through this umbrella header. Listing its internal headers
// would couple the adapter to implementation details without improving include hygiene.
// NOLINTBEGIN(misc-include-cleaner)
#include <sol/sol.hpp>

namespace simple_platformer
{
    namespace
    {
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

        // Leaving the field out keeps the grip, so only a change needs naming.
        ClimbGrip climbGrip(const sol::object& object, std::string_view field)
        {
            if (object.get_type() == sol::type::string)
            {
                const std::string name = object.as<std::string>();
                if (name == "keep")
                {
                    return ClimbGrip::Keep;
                }
                if (name == "hold")
                {
                    return ClimbGrip::Hold;
                }
                if (name == "release")
                {
                    return ClimbGrip::Release;
                }
            }
            throw std::invalid_argument(
                std::string(field) + " must be \"keep\", \"hold\" or \"release\"");
        }

        glm::vec2 vector(const sol::object& object, std::string_view field)
        {
            if (object.get_type() == sol::type::userdata && object.is<glm::vec2>())
            {
                const glm::vec2 value = object.as<glm::vec2>();
                requireFinite(value, std::string(field).c_str());
                return value;
            }
            if (!object.is<sol::table>())
            {
                throw std::invalid_argument(
                    std::string(field) + " must be a vec2 or an {x, y} table");
            }
            const sol::table table = object.as<sol::table>();
            rejectUnknownFields(table, {"x", "y"}, field);
            return {
                number(table.get<sol::object>("x"), std::string(field) + ".x"),
                number(table.get<sol::object>("y"), std::string(field) + ".y")};
        }

        sol::object luaVector(sol::state& lua, glm::vec2 value)
        {
            return sol::make_object(lua, value);
        }
    }

    void rejectUnknownFields(
        const sol::table& table,
        std::initializer_list<std::string_view> allowed,
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
            bool known = false;
            for (std::string_view name : allowed)
            {
                known = known || name == key;
            }
            if (!known)
            {
                throw std::invalid_argument(
                    std::string(subject) + " has unknown field '" + key + "'");
            }
        }
    }

    sol::table luaSnapshot(sol::state& lua, const NpcActivitySnapshot& snapshot)
    {
        sol::table result = lua.create_table();
        result["feet"] = luaVector(lua, snapshot.feet);
        result["targetFeet"] = snapshot.targetFeet.has_value()
                                   ? sol::make_object(lua, luaVector(lua, *snapshot.targetFeet))
                                   : sol::make_object(lua, sol::lua_nil);
        if (snapshot.patrol.has_value())
        {
            result["patrol"] = lua.create_table_with(
                "firstFeet",
                luaVector(lua, snapshot.patrol->firstFeet),
                "secondFeet",
                luaVector(lua, snapshot.patrol->secondFeet));
        }
        else
        {
            result["patrol"] = sol::lua_nil;
        }
        result["stateElapsed"] = snapshot.facts.stateElapsed;
        result["routeComplete"] = snapshot.routeComplete;

        sol::table facts = lua.create_table();
        facts["targetKnown"] = snapshot.facts.targetKnown;
        facts["targetVisible"] = snapshot.facts.targetVisible;
        facts["targetInBiteRange"] = snapshot.facts.targetInBiteRange;
        facts["biteReady"] = snapshot.facts.biteReady;
        facts["targetInSights"] = snapshot.facts.targetInSights;
        facts["targetWithinStandoffDistance"] = snapshot.facts.targetWithinStandoffDistance;
        facts["heardLanding"] = snapshot.facts.heardLanding;
        facts["targetOnSameRun"] = snapshot.facts.targetOnSameRun;
        facts["targetWithinNoticeDistance"] = snapshot.facts.targetWithinNoticeDistance;
        facts["movementBlocked"] = snapshot.facts.movementBlocked;
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
            throw std::invalid_argument("an activity update must return a command table or nil");
        }

        const sol::table table = object.as<sol::table>();
        rejectUnknownFields(
            table,
            {"direction",
             "aimDirection",
             "jumpPressed",
             "jumpHeld",
             "primaryAttackPressed",
             "climbGrip",
             "avoidLedges",
             "contactDamage",
             "routeTo",
             "aimAt",
             "clearRoute"},
            "an activity command");

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
        const auto readClimbGrip = [&](std::string_view name, ClimbGrip& destination)
        {
            const sol::object value = table.get<sol::object>(name);
            if (value.valid() && value.get_type() != sol::type::lua_nil)
            {
                destination = climbGrip(value, std::string("command.") + std::string(name));
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
        readClimbGrip("climbGrip", command.intentions.climbGrip);
        readBoolean("avoidLedges", command.intentions.avoidLedges);
        readBoolean("contactDamage", command.intentions.contactDamage);
        readOptionalVector("routeTo", command.routeTo);
        readOptionalVector("aimAt", command.aimAt);
        readBoolean("clearRoute", command.clearRoute);
        return command;
    }
}

// NOLINTEND(misc-include-cleaner)
