#include "lua_sandbox.hpp"

#include "lua_vec2.hpp"

// sol2 supports its public API through this umbrella header. Listing its internal headers
// would couple the adapter to implementation details without improving include hygiene.
// NOLINTBEGIN(misc-include-cleaner)
#include <lua.hpp>
#include <sol/sol.hpp>

namespace simple_platformer
{
    namespace
    {
        constexpr int InstructionsPerCall = 100'000;

        void instructionBudgetExceeded(lua_State* state, lua_Debug*)
        {
            luaL_error(state, "instruction budget exceeded");
        }
    }

    void openSandbox(sol::state& lua)
    {
        lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
        lua["dofile"] = sol::lua_nil;
        lua["load"] = sol::lua_nil;
        lua["loadfile"] = sol::lua_nil;
        lua["require"] = sol::lua_nil;
        bindVec2(lua);
    }

    InstructionBudget::InstructionBudget(lua_State* state)
        : state(state)
    {
        lua_sethook(state, instructionBudgetExceeded, LUA_MASKCOUNT, InstructionsPerCall);
    }

    InstructionBudget::~InstructionBudget()
    {
        lua_sethook(state, nullptr, 0, 0);
    }
}

// NOLINTEND(misc-include-cleaner)
