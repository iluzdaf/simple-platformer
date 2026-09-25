# Future work

None of the designs in this document are implemented. They are recorded so that
[ARCHITECTURE.md](ARCHITECTURE.md) continues to describe the repository as it is, and
so the reasoning behind a deliberately smaller current design is not lost.

Each proposal states the problem it would solve and sketches an approach. A sketch is not
a commitment, and none of it is required for ordinary project work.

## Level authoring tools

Editing JSON remains useful because the stored level data is visible and reviewable,
but counting columns in a wide tile map makes object placement cumbersome.

The first step should be a read-only level visualiser. It can load the existing JSON
through the normal parser and display row and column rulers together with symbols for
the player, actors, pickups, exits, and patrol points. It should report the same
validation errors as the game and must not introduce another level format.

A later visual editor can let a user paint tiles and place objects, then write the same
validated JSON consumed by the example game. The loader should still produce
`LevelData`, and composition should still create the core's map and world.
Simulation must not depend on the editor or JSON, so handwritten and tool-generated
levels remain equivalent.

## Optional movement abilities

`PlatformerMovement` should remain the readable baseline shared by ordinary ground
actors. Features such as double jump, dash, wall slide, and wall jump can be added as
optional actor components rather than accumulating feature flags inside the baseline
movement component. An actor gains an ability only when its composition includes the
corresponding component, such as `AirJump`, `Dash`, or `WallMovement`.

For example, a double-jump component can contain only its configuration and runtime
state:

```cpp
struct AirJump
{
    int maximumJumps = 1;
    int jumpsRemaining = 1;
};
```

The actor composition makes the feature optional:

```cpp
struct Actor
{
    Body body;
    std::optional<PlatformerMovement> platformerMovement;

    std::optional<AirJump> airJump;
    std::optional<Dash> dash;
    std::optional<WallMovement> wallMovement;
};
```

If several abilities are added, movement can use an explicit ability phase:

```text
InputIntentions
      -> choose or start abilities
      -> produce per-update movement modifiers
      -> apply normal platformer movement and collision
      -> update ability state from collision contacts
```

Abilities should describe changes to the current movement update instead of moving the
body or resolving collision themselves. A small result value can carry intentions and
modifiers such as a velocity override, gravity scale, or whether ordinary horizontal
control and jumping are enabled. For example, an air jump supplies upward velocity, a
dash supplies horizontal velocity and temporarily disables ordinary control, and a wall
slide reduces gravity.

One possible result type is:

```cpp
struct MovementModifiers
{
    bool horizontalControlEnabled = true;
    bool normalJumpEnabled = true;
    float gravityScale = 1.0F;
    std::optional<float> horizontalVelocity;
    std::optional<float> verticalVelocity;
};

struct MovementAbilityResult
{
    InputIntentions intentions;
    MovementModifiers modifiers;
};
```

The actor movement system can then show the complete order directly:

```cpp
MovementAbilityResult abilityResult =
    updateMovementAbilities(actor, intentions, previousContacts, deltaTime);

CollisionContacts contacts = updatePlatformerMovement(
    map,
    actor.body,
    *actor.platformerMovement,
    abilityResult.intentions,
    abilityResult.modifiers,
    actor.facing,
    deltaTime);

finishMovementAbilities(actor, contacts);
```

`PlatformerMovement` would apply the supplied modifiers at named points while retaining
ownership of ordinary acceleration, jumping, gravity, movement, and collision:

```cpp
applyAbilityVelocity(body, modifiers);

if (modifiers.horizontalControlEnabled)
{
    updateHorizontalVelocity(body, movement, intentions, deltaTime);
}
if (modifiers.normalJumpEnabled)
{
    updateJump(body, movement, intentions);
}

updateGravity(body, movement, modifiers.gravityScale, deltaTime);
return moveAndCollide(map, body, deltaTime);
```

Conflicting abilities should be resolved in visible game-policy code with an explicit
priority order; for example, wall jump before air jump when both respond to the jump
button. The post-collision phase can reset air jumps on landing, stop a dash at a wall,
or remember which wall is being touched. Individual abilities should have focused tests,
with a smaller set of integration tests for combinations such as wall slide into wall
jump or an airborne dash into a wall.

The priority should remain ordinary, readable game-policy code:

```cpp
if (canWallJump(actor, previousContacts, intentions))
{
    beginWallJump(actor, result.modifiers);
}
else if (canAirJump(actor, intentions))
{
    beginAirJump(actor, result.modifiers);
}

if (actor.dash.has_value())
{
    updateDash(*actor.dash, actor.facing, intentions, result.modifiers, deltaTime);
}
```

Collision-dependent state is handled afterward:

```cpp
if (contacts.ground && actor.airJump.has_value())
{
    resetAirJumps(*actor.airJump);
}
if ((contacts.left || contacts.right) && actor.dash.has_value())
{
    stopDash(*actor.dash);
}
```

This phase should be introduced alongside the first real optional ability, once its
required data and interactions are concrete. It should not become a callback registry,
inheritance hierarchy, or generic plugin system merely to anticipate possible features.

## NPC tactics

`NpcTactic` has two values, Pursuer and KeepDistance, and the transition table consults
it only for how a known target is pursued. More tactics fit the same shape:

- `Guard`: pursue only inside a home region, then return;
- `Flee`: move away from the target;
- `Patroller`: follow patrol points without pursuing the player.

A tactic that needs a fact the others do not, such as whether a guard is inside its
home region, adds it to `NpcFacts`; one that needs a state the others do not, such as
returning home, adds the state and its function once for every tactic to use. Virtual
brain classes, callbacks, and a general behaviour-tree framework are not needed for
these tactics.

The `Pursuer` tactic should eventually improve how it handles an unreachable target.
Instead of selecting only the geometrically nearest standable cell, it can examine
standable candidates near the last-seen position and return the nearest one for which
pathfinding succeeds. Returning the path and chosen destination together avoids doing
the same search twice:

```cpp
struct ChasePath
{
    NavigationPath path;
    GridPosition destination;
};

std::optional<ChasePath> findClosestReachablePlatformerPath(
    const TileMap& map,
    GridPosition start,
    glm::vec2 targetFeet,
    glm::vec2 bodySize,
    const PlatformerMovementConfig& movement);
```

Candidates should be tried in a deterministic nearest-first order, with path cost used
as a tie-breaker. The existing repath delay can limit the extra searches. The NPC then
follows the returned path and waits at its closest reachable endpoint while remaining
in the chase state: the state expresses its intention to pursue, not a guarantee that
it can reach the target. As with current target memory, this search must use only the
last position the NPC perceived and must not reveal the player's hidden position.
