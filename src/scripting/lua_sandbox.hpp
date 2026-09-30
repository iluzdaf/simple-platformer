#pragma once

// sol2 supports its public API through this umbrella header. Listing its internal headers
// would couple the adapter to implementation details without improving include hygiene.
// NOLINTBEGIN(misc-include-cleaner)
#include <lua.hpp>
#include <sol/sol.hpp>

namespace simple_platformer
{
    // Opens only the base, math, string, and table libraries, removes the chunk loaders,
    // and binds vec2, so scripts can read and compute but not reach files or load code.
    void openSandbox(sol::state& lua);

    // While one lives, a call on the state that runs past its instruction budget fails
    // with an error instead of running on.
    class InstructionBudget
    {
    public:
        explicit InstructionBudget(lua_State* state);
        ~InstructionBudget();

        InstructionBudget(const InstructionBudget&) = delete;
        InstructionBudget& operator=(const InstructionBudget&) = delete;

    private:
        lua_State* state;
    };
}

// NOLINTEND(misc-include-cleaner)
