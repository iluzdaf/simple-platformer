#pragma once

#include <initializer_list>
#include <string_view>

#include "simple_platformer/npc/npc_activity_scripts.hpp"

// sol2 supports its public API through this umbrella header. Listing its internal headers
// would couple the adapter to implementation details without improving include hygiene.
// NOLINTBEGIN(misc-include-cleaner)
#include <sol/sol.hpp>

namespace simple_platformer
{
    // Throws when the table has a key that is not text or is not one of the allowed
    // names. The subject begins the message.
    void rejectUnknownFields(
        const sol::table& table,
        std::initializer_list<std::string_view> allowed,
        std::string_view subject);

    // A fresh Lua table of the snapshot, positions as vec2, so a script changing it never
    // changes the engine's.
    sol::table luaSnapshot(sol::state& lua, const NpcActivitySnapshot& snapshot);

    // The command an update returned: nil asks for nothing. Throws on an unknown field, a
    // wrong type, or a vector, vec2 or {x, y} table, that is not finite.
    NpcActivityCommand commandFrom(const sol::object& object);
}

// NOLINTEND(misc-include-cleaner)
