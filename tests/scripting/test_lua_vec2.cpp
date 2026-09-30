#include <string>

#include <catch2/catch_test_macros.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_activity_script.hpp"
#include "lua_npc_scripts.hpp"

namespace
{
    using simple_platformer::ActorId;
    using simple_platformer::LuaNpcActivity;
    using simple_platformer::LuaNpcScripts;
    using simple_platformer::NpcActivitySnapshot;

    constexpr ActorId FirstActor{1};
    constexpr ActorId SecondActor{2};
    const LuaNpcActivity Activity{"example", "decide"};
}

TEST_CASE("Snapshot positions are vec2 values with glm's arithmetic", "[lua][npc][vec2]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText("example", R"lua(
        return {activities={decide={update=function(self, snapshot)
            local feet = snapshot.feet
            local target = snapshot.targetFeet
            assert(feet.x == 1 and feet.y == 2)
            assert(feet:distanceSquared(target) == 25)
            assert(feet:distance(target) == 5)
            assert((target - feet):length() == 5)
            assert(vec2(1, 2):dot(vec2(3, 4)) == 11)
            assert(vec2(1, 2) + vec2(3, 4) == vec2(4, 6))
            assert(-vec2(1, 2) == vec2(-1, -2))
            assert(2 * vec2(1, 2) == vec2(1, 2) * 2)
            assert(vec2(2, 4) / 2 == vec2(1, 2))
            assert(tostring(vec2(1.5, -2)) == "vec2(1.5, -2)") local moved = vec2(0, 0) moved.x =
        7 return {direction = (target - feet) / 5, aimAt = target, routeTo = moved} end
}
}
}
    )lua");
    NpcActivitySnapshot snapshot;
    snapshot.feet = {1.0F, 2.0F};
    snapshot.targetFeet = {{4.0F, 6.0F}};
    scripts.enter(FirstActor, Activity, snapshot);

    const simple_platformer::NpcActivityCommand command =
        scripts.update(FirstActor, Activity, snapshot, 0.1F);

    REQUIRE(scripts.diagnostics().empty());
    REQUIRE(command.intentions.direction == glm::vec2{0.6F, 0.8F});
    REQUIRE(command.aimAt == glm::vec2{4.0F, 6.0F});
    REQUIRE(command.routeTo == glm::vec2{7.0F, 0.0F});
}

TEST_CASE("A Lua script cannot change vec2 for others", "[lua][npc][vec2]")
{
    LuaNpcScripts scripts;
    // One script tries to replace a method; another shadows vec2 in its own environment.
    scripts.loadScriptText("breaker", R"(
        return {activities={decide={update=function()
            vec2.distanceSquared = function() return 0 end
            return {}
        end}}}
    )");
    scripts.loadScriptText("shadower", R"(
        vec2 = function() return {x = 0, y = 0} end
        return {activities={decide={update=function() return {} end}}}
    )");
    scripts.loadScriptText("example", R"(
        return {activities={decide={update=function()
            return {direction={x=vec2(0, 0):distanceSquared(vec2(3, 4)), y=0}}
        end}}}
    )");
    const NpcActivitySnapshot snapshot;
    const LuaNpcActivity breaker{"breaker", "decide"};
    scripts.enter(FirstActor, breaker, snapshot);
    scripts.update(FirstActor, breaker, snapshot, 0.1F);
    REQUIRE(scripts.diagnostics().size() == 1);
    REQUIRE(scripts.diagnostics().front().message.find("read-only") != std::string::npos);

    scripts.enter(SecondActor, Activity, snapshot);
    REQUIRE(scripts.update(SecondActor, Activity, snapshot, 0.1F).intentions.direction.x == 25.0F);
}

TEST_CASE("A Lua command rejects a vec2 that is not finite", "[lua][npc][vec2]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText("example", R"(
        return {activities={decide={update=function()
            return {aimAt = vec2(math.huge, 0)}
        end}}}
    )");
    const NpcActivitySnapshot snapshot;
    scripts.enter(FirstActor, Activity, snapshot);

    const simple_platformer::NpcActivityCommand command =
        scripts.update(FirstActor, Activity, snapshot, 0.1F);

    REQUIRE_FALSE(command.aimAt.has_value());
    REQUIRE(scripts.diagnostics().size() == 1);
    REQUIRE(
        scripts.diagnostics().front().message.find("command.aimAt must be finite") !=
        std::string::npos);
}
