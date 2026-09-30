#include <initializer_list>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_activity_script.hpp"
#include "simple_platformer/scripting/lua_npc_scripts.hpp"

namespace
{
    using simple_platformer::ActorId;
    using simple_platformer::LuaNpcActivity;
    using simple_platformer::LuaNpcScripts;
    using simple_platformer::NpcActivitySnapshot;

    constexpr ActorId FirstActor{1};
    constexpr ActorId SecondActor{2};
    const LuaNpcActivity Activity{"example", "decide"};

    NpcActivitySnapshot commandSnapshot()
    {
        NpcActivitySnapshot snapshot;
        snapshot.feet = {12.0F, 34.0F};
        snapshot.targetFeet = {{56.0F, 78.0F}};
        snapshot.patrol = simple_platformer::Patrol{{8.0F, 34.0F}, {80.0F, 34.0F}, true};
        snapshot.facts.targetKnown = true;
        snapshot.facts.heardLanding = true;
        snapshot.facts.targetOnSameRun = true;
        snapshot.facts.targetWithinNoticeDistance = true;
        snapshot.facts.targetWithinStandoffDistance = true;
        snapshot.facts.movementBlocked = true;
        snapshot.facts.stateElapsed = 0.25F;
        snapshot.pathComplete = true;
        snapshot.tuning["speed"] = 3.0F;
        return snapshot;
    }
}

TEST_CASE("A Lua activity reads a copied snapshot and returns a command", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example",
        R"(
            return {
                activities = {
                    decide = {
                        update = function(self, snapshot, dt)
                            snapshot.feet.x = 999
                            return {
                                direction = {x = snapshot.tuning.speed * dt, y = 0},
                                aimAt = snapshot.targetFeet,
                                routeTo = snapshot.patrol.secondFeet,
                                primaryAttackPressed = snapshot.facts.targetKnown,
                                climbRequested = snapshot.facts.targetKnown,
                                jumpHeld = snapshot.facts.heardLanding and snapshot.facts.targetOnSameRun,
                                jumpPressed = snapshot.facts.movementBlocked,
                                avoidLedges = snapshot.facts.targetWithinStandoffDistance,
                                contactDamage = snapshot.facts.targetWithinNoticeDistance,
                                clearRoute = snapshot.pathComplete
                            }
                        end
                    }
                }
            }
        )",
        "command.lua");
    NpcActivitySnapshot snapshot = commandSnapshot();
    scripts.enter(FirstActor, Activity, snapshot);

    const simple_platformer::NpcActivityCommand command =
        scripts.update(FirstActor, Activity, snapshot, 0.5F);

    REQUIRE(command.intentions.direction.x == 1.5F);
    REQUIRE(command.intentions.direction.y == 0.0F);
    REQUIRE(command.intentions.primaryAttackPressed);
    REQUIRE(command.intentions.climbRequested);
    REQUIRE(command.intentions.jumpHeld);
    REQUIRE(command.intentions.jumpPressed);
    REQUIRE(command.intentions.avoidLedges);
    REQUIRE(command.intentions.contactDamage);
    REQUIRE(command.aimAt == snapshot.targetFeet);
    REQUIRE(command.routeTo == glm::vec2{80.0F, 34.0F});
    REQUIRE(command.clearRoute);
    REQUIRE(snapshot.feet.x == 12.0F);
    REQUIRE(scripts.diagnostics().empty());
}

TEST_CASE("Lua receives independent run and range facts", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText("example", R"(
        return {activities={decide={update=function(self, snapshot)
            return {jumpHeld=snapshot.facts.targetOnSameRun,
                    contactDamage=snapshot.facts.targetWithinNoticeDistance}
        end}}}
    )");
    NpcActivitySnapshot snapshot;
    scripts.enter(FirstActor, Activity, snapshot);
    for (const bool sameRun : {false, true})
    {
        for (const bool withinRange : {false, true})
        {
            snapshot.facts.targetOnSameRun = sameRun;
            snapshot.facts.targetWithinNoticeDistance = withinRange;
            const auto command = scripts.update(FirstActor, Activity, snapshot, 0.1F);
            REQUIRE(command.intentions.jumpHeld == sameRun);
            REQUIRE(command.intentions.contactDamage == withinRange);
        }
    }
    REQUIRE(scripts.diagnostics().empty());
}

TEST_CASE("Each actor and each visit has its own Lua activity memory", "[lua][npc]")
{
    LuaNpcScripts scripts;
    scripts.loadScriptText(
        "example",
        R"(
            return {
                activities = {
                    decide = {
                        enter = function(self) self.updates = 10 end,
                        update = function(self)
                            self.updates = self.updates + 1
                            return {direction = {x = self.updates, y = 0}}
                        end
                    }
                }
            }
        )");
    const NpcActivitySnapshot snapshot;
    scripts.enter(FirstActor, Activity, snapshot);
    scripts.enter(SecondActor, Activity, snapshot);

    REQUIRE(scripts.update(FirstActor, Activity, snapshot, 0.1F).intentions.direction.x == 11.0F);
    REQUIRE(scripts.update(FirstActor, Activity, snapshot, 0.1F).intentions.direction.x == 12.0F);
    REQUIRE(scripts.update(SecondActor, Activity, snapshot, 0.1F).intentions.direction.x == 11.0F);

    scripts.exit(FirstActor, Activity, snapshot);
    scripts.enter(FirstActor, Activity, snapshot);
    REQUIRE(scripts.update(FirstActor, Activity, snapshot, 0.1F).intentions.direction.x == 11.0F);

    scripts.forget(SecondActor);
    REQUIRE(scripts.update(SecondActor, Activity, snapshot, 0.1F).intentions.direction.x == 0.0F);
    REQUIRE(scripts.diagnostics().back().message == "activity was not entered");
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
