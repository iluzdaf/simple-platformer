#include "lua_vec2.hpp"

#include <sstream>
#include <string>
#include <string_view>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

// sol2 supports its public API through this umbrella header. Listing its internal headers
// would couple the adapter to implementation details without improving include hygiene.
// NOLINTBEGIN(misc-include-cleaner)
#include <sol/sol.hpp>

namespace simple_platformer
{
    namespace
    {
        // Scripts reach vec2 through this read-only constructor, so one script cannot change
        // the type for another.
        constexpr std::string_view Vec2Constructor = R"(
            local Vec2 = vec2
            vec2 = setmetatable({}, {
                __call = function(_, x, y)
                    return Vec2.new(x, y)
                end,
                __newindex = function()
                    error("vec2 is read-only", 2)
                end,
                __metatable = false,
            })
        )";

        std::string vec2Text(glm::vec2 value)
        {
            std::ostringstream text;
            text << "vec2(" << value.x << ", " << value.y << ")";
            return text.str();
        }
    }

    void bindVec2(sol::state& lua)
    {
        lua.new_usertype<glm::vec2>(
            "vec2",
            sol::constructors<glm::vec2(float, float)>(),
            "x",
            sol::property(
                [](const glm::vec2& value) { return value.x; },
                [](glm::vec2& value, float x) { value.x = x; }),
            "y",
            sol::property(
                [](const glm::vec2& value) { return value.y; },
                [](glm::vec2& value, float y) { value.y = y; }),
            sol::meta_function::addition,
            [](glm::vec2 left, glm::vec2 right) { return left + right; },
            sol::meta_function::subtraction,
            [](glm::vec2 left, glm::vec2 right) { return left - right; },
            sol::meta_function::unary_minus,
            [](glm::vec2 value) { return -value; },
            sol::meta_function::multiplication,
            sol::overload(
                [](glm::vec2 value, float scale) { return value * scale; },
                [](float scale, glm::vec2 value) { return value * scale; }),
            sol::meta_function::division,
            [](glm::vec2 value, float scale) { return value / scale; },
            sol::meta_function::equal_to,
            [](glm::vec2 left, glm::vec2 right) { return left == right; },
            sol::meta_function::to_string,
            vec2Text,
            "length",
            [](glm::vec2 value) { return glm::length(value); },
            "distance",
            [](glm::vec2 from, glm::vec2 to) { return glm::distance(from, to); },
            "distanceSquared",
            [](glm::vec2 from, glm::vec2 to) { return glm::dot(to - from, to - from); },
            "dot",
            [](glm::vec2 left, glm::vec2 right) { return glm::dot(left, right); });
        lua.safe_script(Vec2Constructor, "vec2 constructor");
    }
}

// NOLINTEND(misc-include-cleaner)
