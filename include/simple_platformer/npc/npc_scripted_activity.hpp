#pragma once

namespace simple_platformer
{
    struct Actor;
    struct LuaNpcActivity;
    struct NpcBrain;
    struct NpcFacts;
    struct NpcUpdate;
    struct PathFollower;

    // A machine state's Lua activity, run through the update's scripts, which it requires.
    // Each hook hands the script a copied snapshot of the actor, its target and its facts.

    // Drops the old path, as every activity change does, then runs the script's enter.
    void enterScriptedActivity(
        const NpcUpdate& update,
        const Actor& actor,
        const NpcBrain& brain,
        PathFollower& follower,
        const LuaNpcActivity& activity,
        const NpcFacts& facts);

    // Runs the script's update and applies its command as this tick's intentions: a route
    // to follow, a route to drop, and an aim.
    void updateScriptedActivity(
        const NpcUpdate& update,
        Actor& actor,
        const NpcBrain& brain,
        PathFollower& follower,
        const LuaNpcActivity& activity,
        const NpcFacts& facts);

    void exitScriptedActivity(
        const NpcUpdate& update,
        const Actor& actor,
        const NpcBrain& brain,
        const PathFollower& follower,
        const LuaNpcActivity& activity,
        const NpcFacts& facts);
}
