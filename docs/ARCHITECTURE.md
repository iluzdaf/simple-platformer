# Simple Platformer Architecture

This document explains the architecture that exists in the repository now: its main
boundaries, data model, runtime flow, and the reasons behind them.

If this is your first time in the project, follow [START_HERE.md](START_HERE.md) before
reading this document from top to bottom.

Use this as a reference when working on a particular feature:

| Area                      | Section                                                                   | What it covers                                                                                                                                |
| ------------------------- | ------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- |
| Orientation               | [Purpose and scope](#purpose-and-scope)                                   | What the repository is and is not.                                                                                                            |
|                           | [Project shape](#project-shape)                                           | Targets, folders, and the dependency boundary.                                                                                                |
|                           | [Runtime flow](#runtime-flow)                                             | The fixed step and the order systems run in.                                                                                                  |
|                           | [Coordinates](#coordinates)                                               | Axes and feet positions.                                                                                                                      |
|                           | [Time](#time)                                                             | The step, timers, stamps, and which to use.                                                                                                   |
| The data model            | [World ownership and identity](#world-ownership-and-identity)             | What the world owns.                                                                                                                          |
|                           | [Actor composition](#actor-composition)                                   | How capabilities fit together.                                                                                                                |
| Gameplay systems          | [Input and movement](#input-and-movement)                                 | Intentions, platformer and flying movement.                                                                                                   |
|                           | [Tile map, collision, and validation](#tile-map-collision-and-validation) | Terrain and sweeps.                                                                                                                           |
|                           | [NPC behaviour](#npc-behaviour)                                           | Sensing, memory, machines, and Lua activities.                                                                                                |
|                           | [Navigation](#navigation)                                                 | Path search, following, simulated traversals, and the connection cache.                                                                       |
|                           | [Combat, projectiles, and life cycle](#combat-projectiles-and-life-cycle) | Attacks and death.                                                                                                                            |
|                           | [Inventory, pickups, and levels](#inventory-pickups-and-levels)           | The level loop and the [data-driven boundary](#data-driven-level-boundary). [CONTENT.md](CONTENT.md) is the file-by-file authoring reference. |
| Presentation and practice | [Presentation](#presentation)                                             | Animation, rendering, camera, and UI.                                                                                                         |
|                           | [Extension recipes](#extension-recipes-for-project-work)                  | Where to make a gameplay change.                                                                                                              |
|                           | [Error handling and validation](#error-handling-and-validation)           | Which layer rejects what.                                                                                                                     |
|                           | [Testing and quality checks](#testing-and-quality-checks)                 | How to verify it.                                                                                                                             |

## Purpose and scope

Simple Platformer is a small C++17 teaching engine with a complete example game. It
keeps the code explicit enough to trace in a debugger and separates gameplay rules
from graphics so the major paths can be tested without opening a window.

The current example includes:

- responsive platformer movement with variable-height jumping, coyote time, and jump
  buffering
- arbitrary-sized AABB bodies colliding with movement-blocking tiles
- scrolling maps and a dead-zone camera
- actors assembled by composition
- player and NPC control through the same `InputIntentions`
- enum-and-switch NPC state machines, sensing, and target memory
- flying and platformer pathfinding
- 360-degree projectiles and a timed bite attack
- health, death, respawning, pickups, inventory, and three connected levels
- sprite animation, an ImGui HUD, and an optional debug overlay
- data-driven NPC machines which can run protected Lua activities through a copied
  snapshot-and-command boundary

The project deliberately does not try to provide slopes, one-way or moving platforms,
dynamic rigid-body physics, actor pushing, multiplayer, general gameplay scripting beyond
NPC activities, save games, an
editor, an animation graph, a general ECS, or advanced projectile modifiers such as
homing and piercing. OpenGL submission is checked manually rather than with automated
graphics integration tests. Sketches for a few of these, including an editor and
composable movement abilities, are kept in [FUTURE_WORK.md](FUTURE_WORK.md); none of
them are implemented.

## Project shape

### Targets and dependency boundary

The project has four main CMake targets:

- `simple_platformer_core` contains simulation and render-scene construction. It has
  no dependency on GLFW, OpenGL, ImGui, or JSON parsing.
- `simple_platformer_scripting` owns the Lua VM and implements the NPC activity scripting
  boundary without exposing Lua types to the core. It lives on its own in `scripting/`,
  as the application does in `app/`. `lua_npc_scripts.hpp` is its interface; its other
  headers use sol2 and are private to it.
- `simple_platformer` contains the executable, window, input adapter, OpenGL renderer,
  and ImGui presentation.
- `simple_platformer_tests` contains Catch2 tests for the core and for the application
  code that can be tested without a window.

The boundary matters for tests: one can construct a `World`, run movement or a complete
simulation tick, and inspect the result without needing a window or graphics context.

All third-party source is vendored under `external/` so the project builds offline and
everyone works from the same releases. The current dependencies include GLFW, glad, GLM, ImGui,
ImPlot for the debug overlay's plots, imgui-node-editor for its machine window, Catch2,
stb image loading, nlohmann/json, Lua, and sol2. Lua and sol2 are private to the scripting
target rather than leaking through public headers.

### Application folders

The application code is grouped by responsibility:

- `app/game` coordinates the game session and composes playable levels;
- `app/content` contains content definitions, JSON loaders, catalogs, and validators;
- `app/ui` contains player-facing HUD, inventory, and completion UI;
- `app/debug` builds and presents optional debugging information;
- `app/graphics` contains the game window and its OpenGL context, the ImGui session,
  display-viewport conversion, and OpenGL sprite submission.

`application.cpp` owns the outer loop: window events, input collection, fixed updates,
UI, and rendering. `Game` owns the current `GameLevel` and camera controller.
`GameLevel` keeps the level ID, `TileMap`, `World`, and player spawn together.
`Game` translates application input into game input, invokes the engine, builds
the render scene, and replaces the level during a transition.

Example-specific content is kept out of general engine systems. Level geometry and
placements, actors, animations, items, pickup definitions, and exit definitions live in
`assets`. Their loaders and validators live in `app/content`; `app/game/level_composition.cpp`
combines the loaded definitions and placements into a playable level.

### Data, behaviour, and resources

The project uses three simple forms rather than making every concept a class:

- Plain structs hold state, such as `Body`, `PlatformerMovement`, `NpcBrain`, and
  `Sprite`.
- Namespace functions implement behaviour, such as `updatePlatformerMovement` and
  `updateNpcBehaviour`.
- Classes protect owned collections or external resources when invariants and lifetime
  matter, such as `World`, `Inventory`, `CameraController`, and `SpriteRenderer`.

This keeps state visible, makes inputs and outputs explicit, and lets tests call one
piece of behaviour with small values. A class is used when it provides a useful
ownership boundary, not merely to group functions with a familiar subject name.

## Runtime flow

The application gathers input and gives the player's intentions to `Game`.
Simulation advances at a fixed 60 Hz (`FixedDeltaSeconds`, a `double` equal to
`1.0 / 60.0`); rendering runs at the available
frame rate. Frame time is clamped before it enters the fixed-step accumulator so a
breakpoint or stall does not cause an excessive catch-up.

`updateWorldSimulation` is the authoritative gameplay order:

1. Advance the World's shared simulation clock.
2. Hold the player still if they have entered the exit and it is still opening.
3. Update NPC sensing and target memory.
4. Update NPC decisions, goals, paths, and intentions.
5. Move every actor and resolve tile collision.
6. Let pickups fall and resolve their tile collision.
7. Advance attacks and evaluate active bite hitboxes.
8. Move projectiles, find their earliest collision, and break the tiles they destroy.
9. Advance existing projectile bursts and queue expired bursts for removal.
10. Apply damage and advance actor life cycles.
11. Detect automatic pickups.
12. Apply queued world requests.
13. Open the exit when the player enters it with what it needs, and complete the level
    once it has had time to open.

The player intentions are written before this sequence. The camera and
`updateWorldPresentation` (actor animation and cover fades) run afterward on ordinary
gameplay ticks because they present the resulting state.
Level completion takes the transition or completion path instead. Animation updates
change state; they do not issue draw calls.

Systems do not add or erase objects while another system may be traversing their
collections. They append plain values to `WorldRequests`; the requests are applied near
the end of the tick. This makes the mutation point explicit and avoids invalidating
iterators and pointers during a system update.

## Coordinates

- Positive X points right.
- Positive Y points down.
- AABBs and tiles are placed by their top-left corner in world coordinates.
- Actor spawns, pickup placement, exits, patrol points, and navigation destinations use
  world coordinates, usually as feet, described below.
- The internal resolution is 320 by 180 pixels.
- Tiles are square. `tiles.json` declares `tileSize` in world pixels, each `TileMap`
  carries it, and every cell calculation takes that size rather than assuming one. The
  game uses 16.
- Window output is an integer-scaled internal image with letterboxing when required.

A body has two points that code places it by, so the code never uses a general
`setPosition`, which would leave the reader guessing which point it moves. Physics works
with the top-left corner of the body's `Aabb`, where collision measures from. Content and
ground navigation work with its feet, the middle of the bottom edge, where a standing
body meets the ground and where a level author thinks of it standing. Each place names
its point: `topLeft` for the corner, and the feet functions in
[`aabb.hpp`](../include/simple_platformer/math/aabb.hpp) to read a box's feet or to
build or move a box by them.

## Time

Gameplay time is seconds in the fixed simulation step, as `float` except for the world
clock and its stamps. Every system receives the step it ran as `deltaTime`;
`requireSeconds` allows zero but rejects negative or non-finite values. Navigation
simulation requires a positive step through `requirePositiveSeconds`. Rendering has
no step and never advances time.

A moment or a length of time takes one of two forms.

**Timers.** A `float` on the component its window belongs to, ticked once a step by the
one system that owns the component. A countdown is over at zero (`coyoteRemaining`,
`phaseTimeRemaining`, `lifetimeRemaining`, `targetMemoryRemaining`,
`deathTimeRemaining`); a count-up is compared against a length (`stateElapsed`,
`programElapsed`). The length is a duration field beside it, such as
`targetMemoryDuration`. A timer needs no clock, so it is tested by setting the value and
stepping, and not updating its system freezes it. Its cost is order. Where the tick
happens relative to other systems is a rule the reader has to know, which is why the
bite state holds for the update it is entered on.

**Stamps.** An `std::optional<double>` holding the world clock's value when something
happened, empty until it first does (`lastDamageTimeSeconds`, `lastFiredTimeSeconds`,
`lastLockedTouchTimeSeconds`, `openedTimeSeconds`). `World` advances the clock once at
the start of every step. A writer takes `simulationTimeSeconds()`; a reader asks
`secondsSince(stamp)` for its age and compares it against a window the reader owns, so
one write serves every reader. Cover fading reads the shot stamp; hearing uses noise
events. The hit flash's length is a rendering constant. A stamp belongs to the clock
it was taken from. One from the future is rejected, respawn clears the damage stamp, and no
actor carries a stamp into another world. The clock and its stamps are `double`, so a
stamp keeps its precision however long a session runs, and an age is a `float`, being
small.

**Which to use.**

- If one system owns a window's start, ticking, and end, use a timer. Other systems
  may read the remaining duration without advancing it.
- If the question is how long ago something happened, and several systems or rendering
  ask it, use a stamp.
- Rendering reads stamps, the clock, or simulation-owned timers and should not tick them.
- Put a length that content tunes in a duration field in seconds, checked like every
  other time.

## World ownership and identity

`World` owns actors, projectiles, their short-lived burst effects, pickups, item
definitions, the current exit, and the level's platformer connection cache. An actor has a
typed, monotonically increasing
[`ActorId`](../include/simple_platformer/actor/actor_id.hpp) rather than exposing its
vector index. Zero is invalid. IDs are not reused within a world. `World::findActor` performs a
linear search, which is appropriate for the example's small number of actors and keeps
the public model simple.

Pointers returned by `findActor` and references to world-owned data are temporary
views. Adding or removing objects, replacing a level, or restarting can invalidate
them. Code that must remember an actor stores its `ActorId` and performs another lookup
when needed.

Mutable actor and projectile collections let systems update existing component state.
Structural changes still go through `World` or `WorldRequests`, preserving identity and
collection invariants.

## Actor composition

Players and NPCs are configurations of the same [`Actor`](../include/simple_platformer/actor/actor.hpp),
not subclasses. Every actor has a body, intentions, facing, and team. Optional
components supply its capabilities.

### Composition recipe

| Capability     | Components                                               | Rule                                                                    |
| -------------- | -------------------------------------------------------- | ----------------------------------------------------------------------- |
| Movement       | `PlatformerMovement` or `FlyingMovement`                 | Exactly one is required.                                                |
| Climbing       | `SurfaceClimb`                                           | Optional; requires `PlatformerMovement`.                                |
| NPC control    | `NpcBrain`, `NpcPerception`, `NpcSenses`, `PathFollower` | Add these together; a `Patrol` is optional.                             |
| Machine policy | `NpcMachine`                                             | Requires NPC control; chooses activities instead of the brain's tactic. |
| Primary attack | `BiteAttack` or `RangedWeapon`                           | At most one is configured.                                              |
| Contact attack | `ContactDamage`                                          | Can coexist with either primary attack and is requested separately.     |
| Presentation   | `Sprite` and `Animator`                                  | An animator needs a sprite and a complete animation set.                |

The application writes player intentions; NPC activities write the same structure.
An attacking actor needs a non-neutral team for opponent filtering. Health and
inventory are optional components. Reusing a definition with a different spawn or
patrol placement creates another actor without a new C++ type.

`World::addActor` checks component combinations; level validation separately checks
whether actors fit at their authored positions. Systems use the components they
need rather than virtual dispatch.

## Input and movement

### Shared intentions

Player input and NPC decisions produce the same
[`InputIntentions`](../include/simple_platformer/input/input_state.hpp).

An [`InputProgram`](../include/simple_platformer/input/input_program.hpp) holds a timed sequence
of intentions for navigation to replay. The sequence and replay rules belong to input,
not to pathfinding.

Platformer movement reads the horizontal direction; flying movement reads both axes.
Aim is independent of travel direction. Facing is left or right, for sprite flipping and
for which side a bite reaches, and one rule decides it after each movement update: aim
wins when it points left or right, otherwise the way the actor is trying to move,
otherwise it stays as it was. NPCs that look at a target express that as an aim.

`avoidLedges` asks grounded walking to stop before unsupported floor, without changing
its acceleration or preventing a deliberate jump. Platformer movement records whether
a wall or this guard blocked the last update; NPC policy reads that as `movementBlocked`.
`contactDamage` independently asks the combat system to enable body-overlap damage.

The application maps keyboard and mouse state to the player's intentions. Movement and
attack systems do not need separate player and NPC implementations.

GLFW events preserve pressed and released edges until a fixed update consumes them.
The application converts the mouse from window coordinates through the letterboxed
display viewport and camera into a world-space aim direction. Clicks outside the game
viewport are ignored. When ImGui captures input, gameplay input is cleared.

The application can pause the simulation and, while paused, run one fixed step.
Presentation continues while the simulation stands still, and the fixed step is reset
across a pause, as across the inventory, so no burst of catch-up steps follows a
resume. The pause and the step are the application's: the game only sees which steps
it is asked to run. The keys are listed in
[README.md](../README.md#playing-the-example-game).

### Platformer movement

`PlatformerMovement` contains configuration plus a small runtime state for grounded,
coyote, and jump-buffer timing. Its update performs horizontal acceleration or
deceleration, starts a buffered jump when allowed, applies normal or jump-release
gravity, and clamps fall speed.

Movement produces velocity. `Body` then moves by it and stops along whichever axis hit
a tile, one step shared by platformer movement, flying movement, and pickups; movement
observes the contacts that step returns. Gravity and its default rates live with `Body`
too, and the platformer config only overrides them. This direction keeps platformer
rules separate from tile collision and avoids a general ability framework.

An optional [`SurfaceClimb`](../include/simple_platformer/movement/surface_climb.hpp)
lets an actor cling to a climbable tile's wall or underside, moving up and down walls
and sideways along ceilings. `climbGrip` grabs (`Hold`), lets go (`Release`), or leaves
the grip alone (`Keep`, the default), so code that ignores climbing never knocks a
climber off. Letting go or losing contact returns to normal platformer movement.
Navigation routes a climber with the same update; see [traversals](#traversals).

### Flying movement

Flying movement normalises a nonzero two-dimensional intention, multiplies it by the
configured speed, and uses the same tile collision function. It has no gravity,
jumping, or acceleration state.

## Tile map, collision, and validation

`TileMap` stores a rectangular row-major vector of integer tile IDs. Tile zero is empty.
Each nonzero tile definition supplies a sprite region, `blocksMovement`, and
`blocksSight`. The same map layer supports rendering, collision, and sensing.

| Tile  | Movement and projectiles | Sight   | Hides what stands in it |
| ----- | ------------------------ | ------- | ----------------------- |
| Empty | pass                     | passes  | no                      |
| Stone | blocked                  | blocked | nothing can stand in it |
| Glass | blocked                  | passes  | nothing can stand in it |
| Grass | pass                     | blocked | yes                     |

Only a sight-blocking tile that can be walked into hides anything, since nothing can
stand in a tile that blocks movement. Anyone inside grass can see out and across it.
This applies both to NPCs looking for the player and to the player's screen.

On the player's screen, NPCs and pickups fade by how much of their body is in grass:
fully visible while at most half of it is, not drawn from three quarters, and fading
between. Anything the player has a line of sight to is drawn fully. The screen eases
towards that target over `CoverFadeSeconds`, so an NPC revealed when the player steps into
its patch fades in rather than popping. `updateCoverFades` keeps this `screenVisibility`
and `buildRenderScene` draws it. NPCs still see the player by line of sight alone.

The player's own sprite shows whether the world can see them. Their `screenVisibility`
eases towards their own cover fade, raised to fully exposed while any NPC saw them this
update or for `ShotRevealSeconds` after they fire. "Saw them" is read straight from the
`targetVisible` flag that `updateNpcSenses` records in each `NpcPerception`, so gameplay
decides who sees the player once per tick. The renderer draws the player shaded by
`PlayerConcealedShade` rather than faded, so a hidden player darkens instead of
disappearing.

A tile definition may name the tile it `breaksInto`, so breaking swaps a cell's tile ID
instead of adding per-cell state, and a tile that names nothing is unbreakable. A
projectile carries `breaksTiles` from the weapon that fired it and breaks a tile only
when both agree. Glass breaks into empty, the player's weapon breaks tiles, and enemy
weapons do not. `updateProjectiles` takes a mutable map; every other system takes
`const TileMap&`.

Tests construct maps from ASCII strings with a helper in `tests/support` that supplies
its own definitions and symbols, so the engine carries no fixture of its own. The example
loads shared tile definitions separately from level files. Each level's `tileLegend`
maps one-character map symbols to catalog names; there is no default, so a level
says what every symbol it uses means.
The loader resolves names to runtime IDs, reserving zero for `empty`.
Actors, pickups, spawns, and exits are separate level data, not special tile IDs.
Object legend markers expand into these placements during loading; their terrain is empty.

Collision moves an arbitrary-sized AABB along X, resolves it against nearby full-tile
AABBs, then repeats along Y. The result reports left, right, ground, and ceiling
contacts. Actors do not physically collide with or push one another. The left, right,
and bottom map boundaries block movement; the top remains open.

`segmentCast` returns the fraction along a line where it first touches or enters an AABB.
`segmentCastMovementBlockingTiles` applies that operation to the relevant part of a tile map and
can account for a moving box size. Projectiles use movement-blocking tiles;
NPCs use `segmentCastSightBlockingTiles`. Both casts share the geometric calculation
and use the tile property appropriate to their purpose.

After level data is loaded and composed into runtime objects, `validateLevelActors`
checks it against the map.
Every actor spawn, the player's stored respawn, and every patrol endpoint need body
clearance. A platformer's spawn and respawn also need ground support, and so do its
patrol endpoints unless it can climb. A climber's endpoint may be on a wall or
ceiling, since navigation takes it to the nearest place it can hold. Flying actors
never need ground support.
Invalid content fails during loading with the level ID, actor ID, and invalid
location.

## NPC behaviour

### Sensing and memory

`NpcSenses` holds configuration. `NpcPerception` holds transient sensing results
(`targetVisible` and `heardLanding`), replaced on every sensing update. `NpcBrain`
holds decision state and persistent target memory. Behaviour assembles `NpcFacts`
from perception, memory, and other actor components; facts are a policy snapshot,
not another store of sensing state. Debug visibility reads the same perception.

An NPC detects the living player when the player is within its notice distance and a
tile segment cast finds clear line of sight. It stores the player's ID and last known
feet. When sight is lost, a configurable timer lets it continue toward the remembered
position before forgetting the target. Firing gives the player away without making
them visible: every opponent NPC within notice distance hears the shot through any
tiles, remembers the player's feet at that moment, and starts the same timer.
Shots and landings emit `NoiseEvent` values containing the source, kind, and feet at
emission. The next sensing update takes the batch once, offers it to every NPC, then
discards it. Delivery is tied to simulation updates, not render frames or timestamp
tolerances. Moving after an emission does not move the noise. Fresh sight takes priority
over a heard position; otherwise the last eligible noise in the batch refreshes memory.
Landing hearing additionally requires a grounded observer on the same supported run.

Firing also records `RangedWeapon::lastFiredTimeSeconds` on the simulation clock.
Cover fading compares its age with `ShotRevealSeconds`, a duration owned by the
cover-fade module. Combat maintains no reveal countdown. Rendering does not advance
the clock, and consuming noise does not affect the stamp. Hearing never polls this
timestamp; no noise event needs to persist for the reveal or target-memory window.

NPCs pass the last-known feet to navigation as a target point. Navigation leads the
pursuer as close to it as its own collider and capabilities allow; see
[targets](#targets). It never reads the hidden player's current position or moves
either actor directly. Patrol endpoints are targets in the same way.

Sensing only records observations. It does not decide whether to patrol, chase, bite,
or shoot. This keeps perception and decisions separately testable.

### Explicit state machine

Built-in states are declared in [`npc.hpp`](../include/simple_platformer/npc/npc.hpp).
`NpcTactic` selects Pursuer or KeepDistance policy. An update has three steps:

1. `gatherNpcFacts` collects sensing, target memory, movement, attacks, and state time
   into `NpcFacts`. [CONTENT.md](CONTENT.md#state-machines) defines the machine-visible
   facts and their timing.
2. `nextNpcState` in `npc_transitions.cpp` is the transition table: a switch over the
   current state that returns the state to enter, or nothing to stay. It reads only the
   tactic and the facts, so a test hands it those and expects a state. An NPC makes at
   most one transition an update. A known target is pursued from whichever state
   notices it, and a lost one leaves the NPC where the brain's [tactic](#tactics)
   answers.
3. Entering a state resets its time and route. The activity then requests a goal,
   aim, or attack through `InputIntentions`. For example, Chase follows a path to the
   last known target position, while Retreat moves away from it and requests an attack.
   Movement and combat execute those requests later in the same simulation step.

### Tactics

`NpcTactic` is one named policy on the brain: Pursuer or KeepDistance. The transition
table asks it two questions, what to do about a target the NPC knows of and where a
lost one leaves it, and shares every other transition. A Pursuer answers the first with
the attack that reaches, a bite before a shot, or Chase, and the second with Search. A
KeepDistance NPC answers Retreat while the target is nearer than its standoff and
otherwise the same, and watches from where it stands rather than walk to where the
target was. The zombie is a Pursuer. The zombie soldier keeps its distance through the
`keep_distance` machine, the KeepDistance tactic written as data over the same states,
facts and activities.

A tactic chooses; it never adds behaviour. A state, its activity, the facts it decides on
and any capability it uses exist first, so healing instead of attacking is a heal
component, a hurt fact and a Heal state before it is a tactic that answers Heal. Adding
a tactic is an enum value and a branch in each question it answers differently, plus
whatever it needs, added once for every tactic to use.

### Data-driven state machine

An NPC may carry an `NpcMachine` beside its brain, built from a machine in
`machines.json`. When it does, the machine chooses the activity and the tactic is not
asked. A named state runs either a built-in C++ activity or a named Lua activity, and
transitions have a `from`, a `to`, a `when` and an `after`. `when` is a map of fact
names to the value each must hold, answered by the rows in `npc_fact_rows.cpp` over the
same `NpcFacts` the enum brain reads. `after` is how long every condition must hold
before the transition fires; the hold restarts when a condition drops.

`advanceNpcMachine` runs once an update. Among the transitions from the active state,
the first whose conditions have held long enough fires, so a transition's position in
the data is its priority, and at most one fires an update. The machine owns the active
state's elapsed time and calls its activity's exit and enter hooks around a transition.
Built-in values dispatch the existing C++ activity switch; Lua values use the scripting
boundary. Machine activities are not copied into `NpcBrain::state`. Loading rejects a
machine with no states, a repeated state name, a transition
from or to a state it lacks, a condition on a fact no row answers, or a hold that is
not a finite, non-negative time, and names the transition.

The zombie soldier runs built-in activities through the `keep_distance` machine, which
expresses its KeepDistance policy as data.

Behaviour does not move the body directly. If a ground NPC reaches an awkward platform
edge and loses its path, navigation can recover to a supported cell before repathing;
regression tests cover this case.

### Lua activity boundary

The scripting target provides the protected Lua runtime used by scripted machine activities.
The core-facing boundary contains no Lua types. `NpcActivitySnapshot` is a copied,
read-only-in-effect view of position, target, patrol endpoints, facts, state time, route
completion, and tuning.
`NpcActivityCommand` carries only intentions and requests to aim, route, or clear a route.
Applying those requests, including pathfinding, remains engine work.

`LuaNpcScripts` loads each script into its own environment and requires it to return named
activities with an `update` function; `enter` and `exit` are optional. Only the base, math,
string, and table libraries are available, with dynamic loading and filesystem functions
removed. Snapshots become fresh Lua tables whose positions are `glm::vec2` bound as the
Lua value type `vec2`, so scripts do vector arithmetic with the engine's own glm maths
instead of copying helpers they cannot share. A `vec2` is copied in and out, and scripts
reach its constructor through a read-only global, so no script can change the type for
another. Returned command tables reject unknown fields and wrong types, and their vectors,
a `vec2` or an `{x, y}` table, must be finite.

Every visit has a `self` table keyed by stable `ActorId`, script, and activity. Calls are
protected and have an instruction budget. A hook error or invalid command records its source,
script, activity, hook, and actor, then produces no command instead of escaping into the
simulation. A failed script replacement leaves the previous script in place. Before queued actor
removals are applied, an NPC cleanup system discards their script-owned state. Level replacement
and restart discard that state for every actor before replacing the world.

Machine JSON keeps the short string form for built-in activities. A Lua activity uses
`{"kind":"lua","script":"rat","activity":"flee"}`. The application loads referenced
files from `assets/scripts` at startup and rejects missing scripts or activities.
The rat uses Lua to choose a flee goal while C++ follows the path and handles
biting. The spider's Lua patrol and pursuit route it over walls and ceilings the same
way. The boar's Lua charge activity requests ordinary walking, ledge avoidance,
and contact damage; its machine uses facts to choose wake and recovery transitions.
Scripts cannot create noise events or apply damage directly.

## Navigation

Navigation is intentionally the most advanced subsystem, and [START_HERE.md](START_HERE.md)
reads it last. It separates a generic lowest-cost search from the movement-specific
policies that tell the search how cells connect, and it never moves an actor itself: a
path is turned into intentions, and the ordinary movement systems do the moving. This
section starts with the search and movement-specific policies, then explains the
cache and its background fill.

`findActorPath` is the one way in, and with `platformerTraversalProfileFor` the only
public part of `actor_navigation`; the flying and platformer searches it chooses
between are private to it. NPCs pass their actor and a target point.
Navigation reads only the actor's body and its movement
capabilities, never its current movement state. A flyer starts in the cell at its
feet. A platformer starts where its body rests, judged from its bounds alone: the
floor, wall, or ceiling position it matches within a pixel across the surface and
most nearly along it. A body resting nowhere, as in the air, gets no result.

The result is a `NavigationPath` of waypoints. Each waypoint is where the body's feet
rest at the end of a step, how the step is travelled, and the inputs recorded for a
fall, jump, or climb. Cells and surfaces stay inside navigation.

### Targets

A target is a point, and it need not be somewhere the actor can be. It may be
mid-jump, inside a wall, outside the map, or on a platform the actor cannot reach.
The search works in cells only: it heads for the cell that holds the point and stops
at the cheapest node in that cell.

If it reaches the cell, the result is `Found`. If it cannot, the failed search has
visited everything reachable from the start, and the result is `Unreachable` with a
path to the visited node whose cell is nearest the target's; of equally near cells,
the first reached wins. That path has no waypoints when the actor is already there.
Either way the result carries the distance from the path's last waypoint to the
target point, so a caller can judge the outcome for itself. `Deferred` carries no path
and asks for a retry once pending cache work is done. Flying paths never defer.

A cell holding more than one kind of node, such as a floor at the foot of a
climbable wall, ends at whichever the actor reaches first. A climber approaching
from the wall stays on it.

### The search

`route_search` is one A* over locations, a cell and a surface. Flying and ground
policies use only the floor of each cell; climbing policies also use its walls and
ceiling. For each expanded
location it asks the movement policy for outgoing connections, each with a cost and
optional replay inputs. Every cost must be positive, and the heuristic must never
overestimate; a zero heuristic gives Dijkstra's search. The caller supplies a goal
cell and a heuristic from a cell to it. The search stops at the cheapest location in
the goal cell. When it cannot reach it, it returns a path to the reached location
whose cell is nearest. The goal may lie off the grid. A finished search returns that path; the caller
tells whether it reached the goal cell from where it ends. A separate readiness check
can pause the search at a pending location before asking for its connections. A
paused search returns no path, only that location, so the caller can prioritise its
fill.

### Flying paths

Flying navigation treats every cell that allows movement as a node joined to its four
neighbors at a cost of one, with the grid steps between two cells as the heuristic. The path
follower steers straight at each step's cell while collision keeps the body outside
platforms. It shortens the final movement to avoid overshooting the cell's feet point.

### Platformer connections

`platformer_cells` defines where a body can rest. To stand, its cell and the space it
occupies must be clear, with support below. A climber can also rest flush against a
climbable wall beside the cell, or hang from a climbable tile above it. A cell and one
of these surfaces form a location, so the floor, walls, and ceiling of one cell stay
distinct in the search. These rules are all `platformer_cells` exposes. The search
itself finds where a body rests from its bounds, including a supporting cell when its
feet extend past a ledge, and turns its route of locations into the waypoints a
caller receives.

`platformer_connections` builds the connections leaving every location of a cell
the body can rest at. It tries each traversal the profile allows with the real
physics at the caller's step, and each success becomes a connection. The NPC system
passes its current tick's step, so a predicted move and the real one run the same
physics. A connection records the surface it leaves and the surface it reaches, and
the search expands a location with the connections leaving its surface. Costs are
the movement ticks the simulation took.

### Traversals

A traversal that needs an optional capability is tried only for a profile with it.
A new capability adds a row here, its connections in `platformer_connections`, and
its config to the traversal profile. The search itself does not change.

| Traversal | Requires       | Tried from → to                                                                                    | Accepted when                                        | Replayed by the follower                                                                                          |
| --------- | -------------- | -------------------------------------------------------------------------------------------------- | ---------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| Walk      | —              | Floor → each standable floor cell along the row, in both directions                                | The body stops within a pixel of the cell            | Walking and braking into the cell; no inputs are recorded                                                         |
| Fall      | —              | Floor → off the edge beside it                                                                     | It lands on another standable cell and stops there   | Stopping at the takeoff, then the recorded inputs                                                                 |
| Jump      | —              | Floor → a short and a full-height jump each way                                                    | It lands on another standable cell and stops there   | Stopping at the takeoff, then the recorded inputs                                                                 |
| Climb     | `SurfaceClimb` | Floor → wall beside it; wall or ceiling → next cell along it, round a corner, or down to the floor | The body settles at the destination's resting bounds | Holding a surface, travelling it to the start; standing, stopping there; in the air, grabbing on; then the inputs |

A climber whose feet end a climb off its waypoint drops the path, and the NPC plans
again.

The platformer heuristic estimates ticks at the profile's fastest
speed across the whole columns between a cell and the goal cell, since a body's feet
in the one and in the other lie at least that far apart, whatever surface it holds.
Walkers and climbers share it; a new capability only adds its speed. The search adds a fixed jump-start penalty,
also in ticks, so a marginal shortcut does not make a grounded NPC hop.

### The connection cache

A cell's connections depend only on the map, the cell, and a
`PlatformerTraversalProfile` (body size, movement configuration, simulation step,
and optional climb capability), so actors with the same profile can share them
while the map stands.
`PlatformerConnectionCache` in `navigation/platformer_connection_cache` stores, per profile:

- the connections leaving each cell, from every surface of it, with the footprint
  their simulation swept, built by searches or background fill;
- walk results by length, including failed attempts; successful flat-ground walks
  start and end at rest, so their costs can be reused from any cell of any floor.

The `World` owns the cache for the map it is simulated with, since the world is
replaced with its level, and the NPC system hands it to every platformer search.
The platformer search requires the cache and charges each jump its penalty. It only
reads the cache and never simulates or stores connections; the fill is the one thing
that builds them. Optional frame profiling counts searches, expanded cells, and
deferred searches.

### Filling the cache

Background filling uses a queue per profile in `navigation/navigation_fill`. When a
level starts, `queueNavigationFill`
gathers the world's distinct platformer NPC profiles and queues every map cell for
each. Every simulation step begins with a fill phase, `advanceNavigationFill`, that builds and
stores connections for queued cells. Its budget counts simulation ticks plus
a fixed charge per cell, even for cells with no connections. The budget is shared among
profiles with pending cells. A cell is filled as one unit, so its cost can take a
profile past its share of the budget. Filling continues over subsequent steps without
delaying level startup. When a search reaches a cell the cache does not hold yet, it
stops before expanding that cell, queues it if it is not already queued, moves it to
the front, and returns `Deferred`. The NPC asks again next step.
Tests can fill the queue to completion when they need a full cache.

### Breaks

When a projectile breaks a tile, the cache drops only what the break can have
changed. Each cell's connections are cached with a footprint, the rectangle of cells
their simulation swept or read, grown a tile all round for the tiles collision and
support look at beside the body; a broken tile inside a footprint drops that cell.
Walks stay, since no tile decided them.
The map logs the cells it breaks. Searches and fills apply recorded breaks to the cache
before using it, so no separate break notification is needed. The cells a break drops
join the fill queue, and searches that need them wait as they do at a level start. An
NPC also plans again after any break, since its path may have run through the broken
tile.

### Following a path

Path following never teleports an actor or writes its velocity. It emits intentions,
and the ordinary actor movement system performs the motion. A flyer steers at each
waypoint; a platformer follows each waypoint as its [traversal](#traversals) says.
The follower holds a surface while a climb step needs it: it reads its own actor's
climb state to decide how to reach the climb's start, then replays the climb. Between
steps it leaves `climbGrip` at `Keep`, so a climber stays on what it holds. It judges arrival by the feet reaching the waypoint;
a jump or fall is done once the body lands and stops on the waypoint's row, and the
next step walks it the rest of the way. A step that ends away from its waypoint drops
the path, and the NPC plans again. The follower works only in feet positions. It
remembers the goal its path was planned for, and the NPC plans again once the goal
moves more than 8 pixels; the path itself may end short of it. End-to-end tests replay generated
input programs through the real simulation so navigation cannot quietly drift away
from runtime movement.

Targets are points measured against the body's bounds, so neither ground nor flying
navigation depends on where the actor is anchored. NPCs still remember and patrol
by feet, which a floor body's bounds cover at their bottom edge.

## Combat, projectiles, and life cycle

### Primary attacks

`primaryAttackPressed` triggers the configured attack component. A bite and a ranged
weapon remain separate gameplay concepts even though both select the actor's Attack
animation.

A bite moves through Ready, Windup, Active, and Recovery. Its active AABB is placed in
front of the actor according to facing. It has no lunge or knockback, and each eligible
actor can be damaged once per bite. A committed bite completes even if fresh sensing
would no longer start it; the forward hitbox can still miss.

A ranged weapon moves through Ready, Shoot, Recovery, and back to Ready. Entering Shoot
queues exactly one projectile, while the Shoot duration keeps the firing pose visible.
The aim vector supports the full 360-degree range.

### Contact damage

`ContactDamage` is independent of movement and can coexist with a bite or ranged weapon.
While the `contactDamage` intention is held, combat queues body-overlap damage once per
opponent. Releasing it or dying disables damage and clears the hit history, so a later
activation can hit again. It neither moves the actor nor selects an attack animation.
The boar's Lua charge activity combines this request with fast ordinary walking; recovery
requests neither movement nor contact damage.

### Projectiles and deferred damage

A projectile contains bounds, velocity, damage, remaining lifetime, owner, team, and
sprite data. Each tick it performs a swept segment cast from its previous to proposed
position and chooses the earliest movement-blocking tile or eligible-actor hit. An actor
hit queues damage; any hit ends the projectile. Without a hit, it continues until its
lifetime expires. Owner and team prevent hitting the shooter or allies. When a projectile ends,
it queues a separate `ProjectileBurst` at its final position. The burst records whether
the cause was an impact or an expired lifetime. Both causes currently reuse the projectile
sprite and briefly expand and fade, but preserving the cause allows their presentation to
diverge later. A burst cannot collide or deal damage.

Damage is queued rather than applied while attacks and projectiles are being traversed.
`updateLifeState` consumes the requests, records the current simulation time when damage
is applied, and changes an actor from Alive to Dying when health reaches zero.
Render-scene construction turns recent damage into a short white flash. Dying actors
cannot decide, accept gameplay input, attack, or take another hit, but gravity and
collision continue. Their sprites fade during the final part of the explicit death
duration. At the end of the timer an NPC is removed; the player is respawned at its
stored feet position with restored health and movement runtime state. Lifecycle timing
does not depend on the length of an animation clip.

## Inventory, pickups, and levels

These features form one level loop but remain separate subjects:

- `item.hpp` defines item data;
- `inventory.hpp` owns slots and stacking rules;
- `item_use.hpp` applies explicit item effects;
- `pickup.hpp` detects automatic collection;
- `level_exit.hpp` evaluates completion requirements.

An inventory has a configurable slot count. Each slot is empty or contains an item
stack. Adding an item fills compatible stacks, then empty slots, and reports anything
that did not fit. Item effects use an explicit C++ switch rather than a hidden scripting
system.

A pickup has a body like an actor's. It falls at the default gravity and rests on tiles,
so a key placed on glass drops when the glass is shot out from under it. The living
player automatically collects pickups on strict body overlap. NPCs do not. A pickup that
cannot fit completely remains with its uncollected quantity. The example contains coins,
health potions, and a key. Pickup sprites use the shared World clock and a position-based
phase offset to bob without moving their collection bounds. Inventory persists through
player death.

An exit can require an item and optionally consume it. Exit completion is latched so a
requirement cannot be consumed twice. When the living player stands in an exit without
its requirement, the exit records the time on the World clock, and the HUD draws the
required item's icon above the door for a moment after. Entering the exit with the
requirement consumes it and stamps when the door started opening; the player holds still
for `ExitOpenSeconds` while the screen fades them into the flashing door, and then the
level completes. The simulation reports completion;
`GameLevel` groups a level's data so callers cannot accidentally combine parts of
different levels. `Game` replaces that value at a transition, carries over the player's current health and inventory,
and resets the camera.
Velocities, projectiles, NPC state, and old actor IDs do not cross the level boundary.
The final exit shows completion text, and a restart creates a fresh copy of the
catalog's start level.

### Data-driven level boundary

The example game loads its levels from `assets` rather than compiling them in. The
boundary keeps JSON out of the core: `app/content` owns the loaders and validators,
`app/game/level_composition.cpp` combines the results into a playable level, and the core
receives plain C++ values. Levels, actors, items, pickups, exits, and animation sets can
therefore change without touching engine code.

[CONTENT.md](CONTENT.md) is the field-by-field authoring reference for those files.

## Presentation

### Scene construction and rendering

Gameplay objects never issue graphics calls. `buildRenderScene` reads the world and
camera and returns ordered `SpriteDrawCommand` values. The OpenGL `SpriteRenderer`
submits those commands with one small shader. There is no scene graph, material system,
lighting system, or general render graph.

This produces a one-way dependency:

```text
gameplay state -> render scene data -> OpenGL submission
```

Core tests cover scene construction, camera transforms, visible tiles, sprite placement,
facing flips, projectile rotation, and draw order. They do not test the graphics driver.

### The different sizes

The engine keeps visual and physical dimensions separate:

- texture size is the full atlas size in source-image pixels;
- `SpriteRegion` is one source rectangle inside that texture;
- `Sprite::size` is the rectangle drawn in world pixels;
- `Body::bounds.size` is the collision rectangle in world pixels.

A tile has only a `SpriteRegion` and no `Sprite::size`: it always fills one cell, so its
region is the catalog's `tileSize` square and `tiles.json` gives only where it starts.
Every other sprite in the same atlas chooses its world size independently.

Matching sizes are assigned explicitly; the engine does not assume a sprite and body
are equal. Actor sprites are normally positioned from the body's feet, which lets a
tall image use a smaller collider. The bat additionally uses a centred sprite anchor so
its smaller collider matches the creature in the middle of its frame.

A climber's art is drawn once, standing on a floor and facing right. `placeActorSprite`
turns it so its feet rest on the surface it holds: a quarter turn onto a wall and a half
turn onto a ceiling, with the sprite centred along the body's edge against that surface.
The collider does not turn, so a climber whose body is square fits every surface the
same way. The head leads the way the climber faces on a ceiling, and on a wall its
`SurfaceClimb::wallHeading`: the way it last climbed, so it does not turn round when it
stops. Like `facing`, the heading follows the intentions rather than the velocity.

### Animation

Clips are authored in `animations.json`; [CONTENT.md](CONTENT.md#animation-sets) covers
the format. The catalog loads before actors and stays unchanged for the session, and
composition creates a fresh animator for each actor. JSON defines clips, not selection
rules.

There is no animation state machine. A priority function selects a clip from life,
attack, grounded, and velocity state; death has highest priority, then attack. A climber
holding a surface counts as grounded, so it idles or moves there instead of falling.

Each animated actor has an `Animator` with its current animation, elapsed time, and an
`AnimationSet`. Each character therefore owns its clip definitions and can use different
atlas positions and frame counts. `updateWorldAnimations` calls `updateActorAnimations`
to select and advance clips after simulation and write the selected source region to
the actor's `Sprite`. Pickup bobbing, hit flashes, death fading, and projectile bursts
are calculated during scene construction from gameplay state and timers; they do not
all require animation clips.

The supplied atlas is 256 by 256 pixels. The example character clips use fixed 32 by
24 source frames, grouped into named animation sets in `animations.json`.
Artwork sources and atlas tooling live outside this repository; what is here is
the finished runtime atlas.

Frames within a set must share one size. `SpriteRegion` supports arbitrary source
rectangles, but playback changes only the region while `Sprite::size` and its anchor stay
fixed, so mixed-size frames would stretch. Fixed-size example frames avoid this. A richer
`AnimationFrame` carrying a display size and pivot is
[future work](FUTURE_WORK.md); whatever form it takes, collision bodies must remain
independent of animation frame dimensions.

### Camera and display viewport

`CameraController` stores the previous view position. It begins centred on the player,
then moves only enough to return the player's centre to a configurable dead zone. The
camera is clamped to the map and rounded to internal pixels for stable pixel art.

`DisplayViewport` describes where the integer-scaled internal image appears in the
actual framebuffer. Rendering, mouse aiming, HUD layout, and debug UI reuse it so they
agree about letterboxing and high-DPI coordinates.

### HUD and debug overlay

ImGui is used for the health HUD, inventory, completion message, and opt-in debug tools.
`drawInterface` in `app/ui/interface_ui` is the short list of what the player sees over
the scene and the order it is drawn in, as `world_simulation` is for the systems. It is
built before the simulation and hands back what the player asked for as
`InterfaceRequests`, which the loop applies, so a click on the bag pauses the same frame
instead of firing a shot and building the interface never changes the game.
The application owns `DebugToolVisibility`; all control mappings remain in
[Debug overlay](../README.md#debug-overlay). `drawDebugTools` in
`app/debug/debug_tools` orders the optional world, text and machine layers before the
frame panel. `DebugTools` owns the persistent profiling, selection and graph-editor
state, so the application needs one object and one draw call.

`Game::debugOverlay` builds a presentation-ready `DebugOverlay` snapshot without ImGui.
It limits world diagnostics to the camera and a small margin, and carries actor,
projectile, pickup, navigation, camera and machine data. The UI only projects that data
through `DisplayViewport`; it does not change simulation state. This separation keeps
collection and selection logic testable without a window.

`app/debug/machine_graph_ui` presents the selected NPC's `NpcMachine` from that snapshot
without editing it.

Frame profiling is optional. The application owns each frame record and passes it to
the simulation; work measures its own duration with nested scopes and adds named
statistics at the point where it knows them. Parent phases exclude child time, so
the phase totals do not double-count. `FrameHistory` retains completed records for
the debug UI. See `timing/frame_profile` for the API and
[Debug overlay](../README.md#debug-overlay) for its presentation.

The inventory UI is an example presentation, not an engine rule. It derives its rows
from the configured slot count, uses at most three columns, pauses simulation while
open, and emits item use requests instead of changing the world directly.

## Extension recipes for project work

These recipes identify the existing boundaries a project feature should follow. They
are routes through the current code, not requirements for a generic plugin system.

### Adding a movement ability

A movement ability belongs between intentions and collision. It may change velocity,
gravity, or whether ordinary controls are available, but it should not render itself,
edit the tile map, or move the body through a second collision implementation.

For a focused ability:

1. Add any new button edge or held input to `InputState` and `InputIntentions`.
2. Give configuration and runtime state clear names. Keep them separate from input so
   the ability can be driven by either a player or an NPC.
3. Decide visibly how the ability interacts with ordinary horizontal control,
   jumping, gravity, and collision.
4. Apply movement through the existing platformer movement and collision path.
5. Add focused tests for starting, continuing, ending, and resetting the ability, then
   add a small number of interaction tests.
6. Select animation and effects from the resulting state rather than using animation
   frames to drive the mechanic.

A first small feature can extend the existing platformer subject directly. Wall and
ceiling climbing shows the optional form: `SurfaceClimb` is an actor component with its
own configuration and state, and its update falls back to ordinary platformer movement
when the actor holds no surface. Follow it for further abilities instead of filling
`PlatformerMovement` with unrelated flags.

### Adding an NPC state

To arrange existing activities differently, add named states and transitions to a
machine; this needs no new C++ enum value. For a new built-in activity that multiple
NPCs can use, extend the C++ state path:

1. Add the state to the enum.
2. Add any fact its transitions decide on to `NpcFacts`, and gather it in the NPC
   system.
3. Give its entry and exit conditions branches in `nextNpcState`. Entering resets the
   state's timing and clears the path for every state.
4. Let the state's function choose a goal, facing, or attack intention.
5. Continue to move and attack through `InputIntentions`; NPC decision code should not
   write body position or bypass combat systems.
6. Test its transitions with facts alone, then its sustained behaviour and the most
   important interaction with sensing or target memory through `updateNpcBehaviour`.
7. Add the state name to the debug presentation so it can be inspected while playing.
8. Name the activity in the machine loader, so a machine in `machines.json` can run it,
   and give any new fact a row in `npc_fact_rows.cpp`, so a transition can ask for it.

Keep the enum and explicit state branches while the number of states is small. A
behaviour tree, virtual brain hierarchy, or callback registry would make transitions
and state ownership harder to follow without solving a current requirement.
A reusable change to how built-in states are chosen can extend a
[tactic](#tactics); an enemy-specific sequence can stay in its machine.

### Creating a new enemy

First check whether existing components and activities express the enemy. If they do,
add a named definition in `actors.json` and place it in a level. For an enemy whose
policy needs a custom activity, the advanced route is a machine with Lua. Extend the
engine only where that policy needs facts or capabilities it does not already expose:

1. a machine in `machines.json` for new states and transitions;
2. a Lua activity in `assets/scripts` for policy that existing activities cannot express;
3. a C++ fact when the policy needs an observation the engine does not yet supply;
4. a C++ component or system when the engine lacks a movement or combat capability;
5. animation frames and an animation set when the enemy needs new presentation;
6. focused tests for new engine rules and interactions, while content-integrity tests
   check that shipped references resolve.

Species, capabilities, and decisions are separate concerns. Artwork does not determine
the brain, and a ranged weapon needs no `Shooter` subclass. The decision policy is
the brain's [tactic](#tactics) or its machine. Add a new tactic only for a policy shared
by multiple actors; it may need new facts or built-in states.

### Choosing the layer

| Change                                                                         | Primary location                  |
| ------------------------------------------------------------------------------ | --------------------------------- |
| Input binding or mouse conversion                                              | `app/application.cpp`             |
| Movement or collision rule                                                     | `src/movement` or `src/physics`   |
| NPC perception or decision                                                     | `src/npc`                         |
| Generic search or movement-specific connections                                | `src/navigation`                  |
| Damage, attacks, or projectiles                                                | `src/combat`                      |
| Animation definitions                                                          | `assets/catalogs/animations.json` |
| Content loading and validation                                                 | `app/content`                     |
| Playable level composition and session flow                                    | `app/game`                        |
| Actor, tile, item, pickup, and exit definitions; level geometry and placements | `assets`                          |
| HUD or debugging presentation                                                  | `app/ui` or `app/debug`           |

When a feature crosses layers, keep its rule in the simulation and pass plain state to
presentation. Add the smallest test at the layer that owns the rule before adding an
end-to-end test.

## Error handling and validation

Validation has three boundaries:

1. **JSON shape:** loaders check types, integer ranges, required fields, and reject unknown
   fields to catch misspellings. This includes catalogs, level placements, and legend
   templates. `content_json` provides shared file-reading and shape-checking helpers.
   `content_diagnostics` builds the field paths and raises the errors, and carries no
   JSON dependency so the C++ validators can use it too.
   `readInteger`, `readNumber`, `readBoolean`, `readText`, and `readVector` read
   required fields. Their `readOptional...` counterparts leave C++ defaults unchanged
   only when a field is absent; present but invalid values are errors. Both use the
   same `json...` value checks and accept a source filename and field path.
2. **Application content:** plain C++ validators check authoring rules.
   [`content_validation.cpp`](../app/content/content_validation.cpp) covers legends, map rows,
   placement counts, quantities, and exit settings. Actor, item, pickup, and exit catalog
   validators check their definitions, including unused entries. Composition resolves
   cross-file names and adds the originating field to reference errors.
3. **Core invariants:** validators such as
   [`validateActor`](../src/actor/actor_validation.cpp) and
   [`validatePickup`](../src/world/pickup.cpp) check runtime values regardless of how they
   were created. [`validateLevelActors`](../src/world/level_validation.cpp) then checks
   actor clearance and support against the actual map.

These are separate responsibilities: reading valid JSON does not establish that an
actor fits on a platform. Domain checks are callable without parsing JSON; loaders
add source context to their errors.

Invalid programmer or content input throws descriptive exceptions at a boundary where
it can be explained clearly. Examples include invalid dimensions, non-finite values,
unknown item IDs, invalid actor composition, and unsupported spawn positions.

Required assets do not silently fall back to placeholders. Startup catches exceptions
once, prints the message, and exits. Runtime systems assume successfully constructed
objects already satisfy their documented invariants.

## Testing and quality checks

Tests mirror the source subjects and focus on behaviour rather than private
implementation. Important coverage includes:

- coordinate and feet conversions;
- fixed-step accumulation and input-edge consumption;
- actor identity, lookup, removal, and deferred requests;
- platformer and flying movement;
- arbitrary body sizes, four-sided tile collision, corners, and map boundaries;
- camera dead-zone following, clamping, centring, and pixel rounding;
- NPC sensing, memory, FSM transitions, continuous patrol, and edge recovery;
- lowest-cost search, heuristics, flying paths, standability, falls, replayed jump
  programs, the connection cache, breaks and the fill;
- bite and ranged attack phases;
- swept projectiles, teams, damage, death, removal, and respawn;
- inventory stacking and capacity, automatic pickup, item use, exit requirements, and
  level transitions;
- animation selection and render-scene generation;
- supplied level validation and invalid-content diagnostics.

Tests that need an actor usually define a small local factory containing only the data
relevant to that subject. This duplication is intentional: each test remains readable
without discovering a large shared fixture full of unrelated defaults.

For a new rule, start beside the code you changed:

| Change                                     | Test starting point                          |
| ------------------------------------------ | -------------------------------------------- |
| Content parsing or definition validation   | `tests/app/content/`                         |
| Level parsing, validation, and diagnostics | `tests/app/content/test_level_data*.cpp`     |
| Composing catalog entries into levels      | `tests/app/game/test_level_*composition.cpp` |
| Carrying player state between levels       | `tests/app/game/test_level_transition.cpp`   |
| Pickup collection and movement             | `tests/world/test_pickups.cpp`               |
| Exit requirements and completion           | `tests/world/test_level_exit.cpp`            |
| Item use and inventory persistence         | `tests/world/test_world_inventory.cpp`       |
| Level-object draw commands                 | `tests/render/test_level_object_render.cpp`  |
| Behaviour involving multiple systems       | `tests/world/test_world_simulation.cpp`      |
| NPC behaviour across a simulation step     | `tests/world/test_npc_world_simulation.cpp`  |
| Visual state converted to draw commands    | `tests/render/test_render_scene.cpp`         |

Use small independent data in tests rather than asserting the example campaign's
enemy count, item values, or inventory capacity. Its own checks should test validity,
so you can change content without rewriting unrelated tests.

OpenGL and ImGui integration remain a manual run; automated graphics-context tests are
avoided. [README.md](../README.md#continuous-integration) lists what CI checks.
