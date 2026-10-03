#pragma once

namespace simple_platformer
{
    class TileMap;
    struct Actor;
    struct NpcBrain;
    struct NpcPerception;

    // What transitions know this update: observations, remembered targets, movement,
    // attacks, and elapsed times. Gathering these first keeps state decisions testable.
    struct NpcFacts
    {
        // A living target is remembered, seen or not.
        bool targetKnown = false;
        bool targetVisible = false;
        // The target is visible and inside the bite's hitbox.
        bool targetInBiteRange = false;
        bool biteReady = false;
        // The target is visible and the NPC has a ranged weapon.
        bool targetInSights = false;
        // The living target's last known feet are strictly within the senses'
        // standoff distance of the NPC's current feet.
        bool targetWithinStandoffDistance = false;
        bool heardLanding = false;
        // These two facts use the living target's current bounds, even without sight.
        // Charger uses them to decide whether another charge is possible.
        bool targetOnSameRun = false;
        bool targetWithinNoticeDistance = false;
        bool movementBlocked = false;
        bool hasPatrol = false;
        // Whether searching is enabled, and whether its configured duration has elapsed.
        bool searches = false;
        bool searchTimeUp = false;
        float stateElapsed = 0.0F;
        float targetLostElapsed = 0.0F;
    };

    // The facts for this NPC now. The target is its living remembered target, if any, and
    // the state has been active for stateElapsed.
    NpcFacts gatherNpcFacts(
        const TileMap& map,
        const Actor& actor,
        const NpcBrain& brain,
        const NpcPerception& perception,
        const Actor* target,
        float stateElapsed);
}
