#include <initializer_list>

#include <catch2/catch_test_macros.hpp>
#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/npc/npc.hpp"
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
        snapshot.routeComplete = true;
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
                                climbGrip = snapshot.facts.targetKnown and "hold" or "release",
                                jumpHeld = snapshot.facts.heardLanding and snapshot.facts.targetOnSameRun,
                                jumpPressed = snapshot.facts.movementBlocked,
                                avoidLedges = snapshot.facts.targetWithinStandoffDistance,
                                contactDamage = snapshot.facts.targetWithinNoticeDistance,
                                clearRoute = snapshot.routeComplete
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
    REQUIRE(command.intentions.climbGrip == simple_platformer::ClimbGrip::Hold);
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
