#pragma once

// sol2 supports its public API through this umbrella header. Listing its internal headers
// would couple the adapter to implementation details without improving include hygiene.
// NOLINTBEGIN(misc-include-cleaner)
#include <sol/sol.hpp>

namespace simple_platformer
{
    // Binds glm::vec2 as the Lua value type vec2: x and y, the arithmetic operators, ==,
    // tostring, and length, distance, distanceSquared, and dot. A vec2 is copied in and
    // out, so a script changing one never changes the engine's. Scripts reach its
    // constructor, vec2(x, y), through a read-only global, so one script cannot change the
    // type for another.
    void bindVec2(sol::state& lua);
}

// NOLINTEND(misc-include-cleaner)
