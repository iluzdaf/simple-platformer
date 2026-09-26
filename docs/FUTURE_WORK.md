# Future work

Most of the designs in this document are not implemented. A proposal states when groundwork
already exists. They are recorded so that
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

## Lua-authored NPC behaviour

The protected Lua runtime, copied snapshot-and-command boundary, and machine activity
integration are implemented. No shipped actor uses Lua yet; the remaining work adds the
behaviours and engine capabilities below.

Lua can add game-specific decisions without moving simulation mechanics out of the
engine. It should extend the existing teaching progression rather than replace it:

1. The zombie keeps the explicit enum-and-switch state machine in C++, so a student can
   see facts become transitions and activities directly in code.
2. The zombie soldier keeps its machine in `machines.json`, showing the same kind of
   transition graph as validated data over built-in C++ activities.
3. The rat, boar, and spider can then show scripted state policy over capabilities
   supplied by the engine.

The boundary should be that Lua decides what an NPC wants to do, while C++ determines
how it is done. A scripted activity can have `enter`, `update`, and `exit` functions,
with fresh state-local memory on entry. `update` receives an immutable snapshot of
facts and tuning and returns a small command value: intentions, an aim or destination,
an attack request, or a request to follow a route. It must not move bodies, resolve
collision, search the navigation graph, apply damage, or add and remove world objects.

Machine-state identity is separated from the activity that implements it. A state can
run either a named built-in C++ activity or a named Lua activity. The existing C++
transition runner, validation, timing, priority, and debug view work for both. The
existing JSON format remains useful; if Lua later authors a machine, the
script should return declarative states and transitions which are converted to the same
validated `NpcStateMachine`, rather than execute unrestricted transition callbacks every
update.

The representation keeps `NpcState` as the identifier understood by the existing C++
activity switch:

```cpp
struct BuiltInNpcActivity
{
    NpcState state;
};

struct LuaNpcActivity
{
    std::string script;
    std::string activity;
};

using NpcActivity = std::variant<BuiltInNpcActivity, LuaNpcActivity>;

struct NpcMachineState
{
    std::string name;
    NpcActivity does;
};
```

`BuiltInNpcActivity{NpcState::Patrol}` means to run the existing Patrol branch; it does
not introduce another implementation of patrol. A Lua activity instead identifies a
function table in a loaded script. The current string form can remain the concise form
for a built-in activity, preserving the zombie soldier as the intermediate example:

```json
{ "name": "patrol", "does": "patrol" }
```

A scripted rat state uses an explicit tagged form so loading can validate its script
and activity:

```json
{
  "name": "fleeing",
  "does": { "kind": "lua", "script": "rat", "activity": "flee" }
}
```

The two decision paths should remain visible. An NPC without an `NpcMachine` uses
`nextNpcState` and the explicit C++ transition switch, then runs the built-in activity
for its `NpcBrain::state`. A machine-controlled NPC advances its declarative graph and
dispatches the active state's `NpcActivity`: a built-in value calls that same C++
activity switch, while a Lua value calls its `enter`, `update`, or `exit` hook. Both
paths ultimately produce intentions or narrow engine commands consumed by the same
navigation, movement, and combat systems.

The machine owns its active state's elapsed time and activity lifecycle. A Lua state
cannot be represented honestly by `NpcBrain::state`, so machine-controlled NPCs do not
copy their activity back into that enum. The debug overlay instead shows the machine
state name and an activity label such as `builtin: patrol` or `lua: rat.flee`.

The old Platformer provides a useful order for introducing the enemies, but its code
should be adapted to this engine rather than copied:

- The rat is the smallest first scripted NPC. Lua chooses patrol and flee destinations,
  while the C++ path follower walks there and a C++ pounce activity owns the leap,
  collision, and attack consequences.
- The boar adds event-driven policy and the Sleep, Charge, and Stunned states. Sensing,
  charge movement, collision, and damage remain C++; Lua reacts to engine-provided facts
  such as a heard noise or nearby target. New facts should be added deliberately to the
  immutable snapshot rather than letting scripts create arbitrary world truth.
- The spider comes after surface movement and navigation are engine capabilities. Lua
  chooses whether and where to patrol, chase, or pounce; C++ gets it across floors,
  walls, ceilings, and corners.

Wall and ceiling crawling is therefore a prerequisite for the spider, not part of the
Lua integration. It needs explicit C++ movement state and collision contacts, navigation
connections between compatible floor, wall, and ceiling surfaces, and a path follower
that turns those connections into intentions. The same capability should be testable
without Lua before a scripted spider uses it.

Script calls have protected error handling and an instruction budget. A failed update reports
the script, actor, activity, and hook, then produces no commands for that update instead of
damaging the simulation. Loading rejects missing required hooks and unknown machine activity
references, and command parsing rejects unknown fields and invalid values. State-local memory
is discarded with its actor.

A practical remaining delivery order is:

1. Add a C++ pounce activity and implement the rat using existing ground movement and
   navigation.
2. Add the event and fact boundary needed by the boar, then implement its charge in C++
   and its policy in Lua.
3. Implement and test wall and ceiling movement, surface navigation, and following in
   C++.
4. Implement the spider as the first scripted user of that surface-navigation
   capability.
5. Consider Lua-authored declarative machine definitions only after the state boundary
   has proved useful; keep runtime transition evaluation in C++ unless a concrete rule
   cannot be expressed by facts and timed conditions.

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

## Closest reachable chase destination

Chase should eventually improve how it handles an unreachable target. Instead of
selecting only the geometrically nearest standable cell, it can examine standable
candidates near the last-seen position and return the nearest one for which pathfinding
succeeds. Returning the path and chosen destination together avoids doing the same
search twice:

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
