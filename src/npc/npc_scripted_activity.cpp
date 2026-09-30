#include "simple_platformer/npc/npc_scripted_activity.hpp"

#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_activities.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_activity_script.hpp"
#include "simple_platformer/npc/npc_facts.hpp"
#include "simple_platformer/npc/npc_navigation.hpp"
#include "simple_platformer/npc/npc_update.hpp"

namespace simple_platformer
{
    namespace
    {
        NpcActivitySnapshot activitySnapshot(
            const Actor& actor,
            const NpcBrain& brain,
            const PathFollower& follower,
            const NpcFacts& facts)
        {
            NpcActivitySnapshot snapshot;
            snapshot.feet = feetOf(actor.body.bounds);
            snapshot.patrol = actor.patrol;
            snapshot.facts = facts;
            snapshot.routeComplete = pathComplete(follower);
            if (facts.targetKnown)
            {
                snapshot.targetFeet = brain.lastKnownTargetFeet;
            }
            return snapshot;
        }

        NpcActivityScripts& requiredScripts(const NpcUpdate& update)
        {
            if (update.scripts == nullptr)
            {
                throw std::logic_error("A scripted NPC activity needs the scripting runtime");
            }
            return *update.scripts;
        }

        void applyScriptCommand(
            const NpcUpdate& update,
            Actor& actor,
            PathFollower& follower,
            const NpcActivityCommand& command)
        {
            actor.intentions = command.intentions;
            if (command.clearRoute)
            {
                clearPath(follower);
            }
            if (command.routeTo.has_value())
            {
                const InputIntentions movement =
                    intentionsToReach(update, actor, follower, *command.routeTo);
                actor.intentions.direction = movement.direction;
                actor.intentions.jumpPressed = movement.jumpPressed;
                actor.intentions.jumpHeld = movement.jumpHeld;
                actor.intentions.climbGrip = movement.climbGrip;
            }
            if (command.aimAt.has_value())
            {
                aimToward(actor, *command.aimAt);
            }
        }
    }

    void enterScriptedActivity(
        const NpcUpdate& update,
        const Actor& actor,
        const NpcBrain& brain,
        PathFollower& follower,
        const LuaNpcActivity& activity,
        const NpcFacts& facts)
    {
        clearPath(follower);
        requiredScripts(update).enter(
            actor.id, activity, activitySnapshot(actor, brain, follower, facts));
    }

    void updateScriptedActivity(
        const NpcUpdate& update,
        Actor& actor,
        const NpcBrain& brain,
        PathFollower& follower,
        const LuaNpcActivity& activity,
        const NpcFacts& facts)
    {
        const NpcActivityCommand command = requiredScripts(update).update(
            actor.id, activity, activitySnapshot(actor, brain, follower, facts), update.deltaTime);
        applyScriptCommand(update, actor, follower, command);
    }

    void exitScriptedActivity(
        const NpcUpdate& update,
        const Actor& actor,
        const NpcBrain& brain,
        const PathFollower& follower,
        const LuaNpcActivity& activity,
        const NpcFacts& facts)
    {
        requiredScripts(update).exit(
            actor.id, activity, activitySnapshot(actor, brain, follower, facts));
    }
}
