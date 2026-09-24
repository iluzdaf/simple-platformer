# Simple Platformer Architecture

This document explains the architecture that exists in the repository now: its main
boundaries, data model, runtime flow, and the reasons behind them.

If this is your first time in the project, follow [START_HERE.md](START_HERE.md) before
reading this document from top to bottom.

Use this as a reference when working on a particular feature:

| Area | Section | What it covers |
| --- | --- | --- |
| Orientation | [Purpose and scope](#purpose-and-scope) | What the repository is and is not. |
| | [Project shape](#project-shape) | Targets, folders, and the dependency boundary. |
| | [Runtime flow](#runtime-flow) | The fixed step and the order systems run in. |
| | [Coordinates and time](#coordinates-and-time) | Axes, feet positions, and the shared clock. |
| The data model | [World ownership and identity](#world-ownership-and-identity) | What the world owns. |
| | [Actor composition](#actor-composition) | How capabilities fit together. |
| Gameplay systems | [Input and movement](#input-and-movement) | Intentions, platformer and flying movement. |
| | [Tile map, collision, and validation](#tile-map-collision-and-validation) | Terrain and sweeps. |
| | [NPC behaviour](#npc-behaviour) | Sensing, memory, and the explicit state machine. |
| | [Navigation](#navigation) | Path search, following, and simulated jumps. |
| | [Combat, projectiles, and life cycle](#combat-projectiles-and-life-cycle) | Attacks and death. |
| | [Inventory, pickups, and levels](#inventory-pickups-and-levels) | The level loop and the [data-driven boundary](#data-driven-level-boundary). [CONTENT.md](CONTENT.md) is the file-by-file authoring reference. |
| Presentation and practice | [Presentation](#presentation) | Animation, rendering, camera, and UI. |
| | [Extension recipes](#extension-recipes-for-project-work) | Where to make a gameplay change. |
| | [Error handling and validation](#error-handling-and-validation) | Which layer rejects what. |
| | [Testing and quality checks](#testing-and-quality-checks) | How to verify it. |

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

The project deliberately does not try to provide slopes, one-way or moving platforms,
dynamic rigid-body physics, actor pushing, multiplayer, scripting, save games, an
editor, an animation graph, a general ECS, or advanced projectile modifiers such as
homing and piercing. OpenGL submission is checked manually rather than with automated
graphics integration tests. Sketches for a few of these, including an editor and
composable movement abilities, are kept in [FUTURE_WORK.md](FUTURE_WORK.md); none of
them are implemented.

## Project shape

### Targets and dependency boundary

The project has three main CMake targets:

- `simple_platformer_core` contains simulation and render-scene construction. It has
  no dependency on GLFW, OpenGL, ImGui, or JSON parsing.
- `simple_platformer` contains the executable, window, input adapter, OpenGL renderer,
  and ImGui presentation.
- `simple_platformer_tests` contains Catch2 tests for the core and for the application
  code that can be tested without a window.

The boundary matters for tests: one can construct a `World`, run movement or a complete
simulation tick, and inspect the result without needing a window or graphics context.

All third-party source is vendored under `external/` so the project builds offline and
everyone works from the same releases. The current dependencies include GLFW, glad, GLM, ImGui,
ImPlot for the debug overlay's plots, Catch2, stb image loading, and nlohmann/json.

### Application folders

The application code is grouped by responsibility:

- `app/game` coordinates the game session and composes playable levels;
- `app/content` contains content definitions, JSON loaders, catalogues, and validators;
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
4. Update NPC decisions, destinations, paths, and intentions.
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

## Coordinates and time

- Positive X points right.
- Positive Y points down.
- AABBs and tiles use a top-left world position.
- Actor spawns, pickup placement, exits, patrol points, and navigation destinations use
  world coordinates; actor and navigation placement helpers commonly use feet, the
  bottom centre of an actor body.
- The internal resolution is 320 by 180 pixels.
- Tiles are square. `tiles.json` declares `tileSize` in world pixels, each `TileMap`
  carries it, and every cell calculation takes that size rather than assuming one. The
  game uses 16.
- Window output is an integer-scaled internal image with letterboxing when required.
- `World` owns elapsed simulation time. It advances once per fixed simulation update and
  provides a shared clock for effects that do not need their own resettable timer.
  Actors store damage timestamps against this clock, while rendering decides how recent
  damage should look. `World::secondsSince` answers how long ago such a stamp was, and
  rejects one from the future, so the readers only compare the age to their window.

The two actor-position conventions are deliberately named:

```cpp
struct Aabb
{
    glm::vec2 position; // top-left world position
    glm::vec2 size;
};

glm::vec2 feetOf(const Aabb& box);
void placeFeetAt(Aabb& box, glm::vec2 feet);
```

Physics code works with `body.bounds.position`. Content and ground navigation use
`feetOf` and `placeFeetAt`. There is no ambiguous general `setPosition` function.

## World ownership and identity

`World` owns actors, projectiles, their short-lived burst effects, pickups, item
definitions, the current exit, and the platformer connections its searches have found
for the level's map. An actor has a typed, monotonically increasing ID
rather than exposing its vector index:

```cpp
struct ActorId
{
    std::uint32_t value = 0;
};
```

Zero is invalid. IDs are not reused within a world. `World::findActor` performs a
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

Player and NPC are roles built from the same `Actor` aggregate, not subclasses:

```cpp
struct Actor
{
    ActorId id;
    Body body;
    InputIntentions intentions;
    std::optional<PlatformerMovement> platformerMovement;
    std::optional<FlyingMovement> flyingMovement;
    Facing facing = Facing::Right;

    LifeState life = LifeState::Alive;
    float deathTimeRemaining = 0.0F;
    std::optional<float> lastDamageTimeSeconds;

    std::optional<Sprite> sprite;
    std::optional<Animator> animator;
    std::optional<Health> health;
    std::optional<Inventory> inventory;
    Team team = Team::Neutral;
    std::optional<RangedWeapon> rangedWeapon;
    std::optional<BiteAttack> bite;
    std::optional<NpcBrain> brain;
    std::optional<NpcSenses> senses;
    std::optional<Patrol> patrol;
    std::optional<PathFollower> pathFollower;
};
```

### Composition recipe

When creating a new actor, start with `Body` and add only the data required by its
capabilities:

| Role | Movement | Control | Combat and life | Presentation |
| --- | --- | --- | --- | --- |
| Player | `PlatformerMovement` | application writes `InputIntentions` | `Health`, `Inventory`, `Team::Player`, `RangedWeapon` | `Sprite`, `Animator` |
| Zombie | `PlatformerMovement` | `NpcBrain`, `NpcSenses`, `Patrol`, `PathFollower` | `Health`, `Team::Enemy`, `BiteAttack` | `Sprite`, `Animator` |
| Bat | `FlyingMovement` | `NpcBrain`, `NpcSenses`, `Patrol`, `PathFollower` | `Health`, `Team::Enemy`, `BiteAttack` | `Sprite`, `Animator` |
| Zombie soldier | `PlatformerMovement` | `NpcBrain`, `NpcSenses`, `Patrol`, `PathFollower` | `Health`, `Team::Enemy`, `RangedWeapon` | `Sprite`, `Animator` |

The recipe is additive. For example, making a second zombie does not require another
type: reference the same definition with different spawn and patrol data. A bat can use a
smaller body while keeping a larger sprite because physical and visual sizes are
independent.

The important composition rules are:

- an actor has exactly one movement component;
- an NPC needs a brain, senses, and path follower; add a patrol only when it should
  move between authored patrol points;
- an actor has at most one configured primary attack in the example;
- attacks use a non-neutral team so friend-or-foe filtering is defined;
- an `Animator` is useful only with a `Sprite` and a complete `AnimationSet`.

`World::addActor` rejects combinations that would make runtime dispatch ambiguous;
level validation separately checks whether composed actors fit at their authored
positions. Systems check for the component they operate on rather than using virtual
methods or an inheritance hierarchy.

## Input and movement

### Shared intentions

```cpp
struct InputIntentions
{
    glm::vec2 direction = {0.0F, 0.0F};
    glm::vec2 aimDirection = {0.0F, 0.0F};
    bool jumpPressed = false;
    bool jumpHeld = false;
    bool primaryAttackPressed = false;
};
```

Platformer movement reads the horizontal direction; flying movement reads both axes.
Aim is independent of travel direction. Facing is left or right, for sprite flipping and
for which side a bite reaches, and one rule decides it after each movement update: aim
wins when it points left or right, otherwise the way the actor is trying to move,
otherwise it stays as it was. NPCs that look at a target express that as an aim.

The application maps keyboard and mouse state to the player's intentions. NPC systems
write the same structure from their decisions. Movement and attack systems therefore
do not need separate player and NPC implementations.

GLFW events preserve pressed and released edges until a fixed update consumes them.
The application converts the mouse from window coordinates through the letterboxed
display viewport and camera into a world-space aim direction. Clicks outside the game
viewport are ignored. When ImGui captures input, gameplay input is cleared.

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

### Flying movement

Flying movement normalises a nonzero two-dimensional intention, multiplies it by the
configured speed, and uses the same tile collision function. It has no gravity,
jumping, or acceleration state.

## Tile map, collision, and validation

`TileMap` stores a rectangular row-major vector of integer tile IDs. Tile zero is empty.
Each nonzero tile definition supplies a sprite region, `blocksMovement`, and
`blocksSight`. The same map layer supports rendering, collision, and sensing.

| Tile | Movement and projectiles | Sight | Hides what stands in it |
| --- | --- | --- | --- |
| Empty | pass | passes | no |
| Stone | blocked | blocked | nothing can stand in it |
| Glass | blocked | passes | nothing can stand in it |
| Grass | pass | blocked | yes |

Only a sight-blocking tile that can be walked into hides anything, since nothing can
stand in a tile that blocks movement. Anyone inside grass can see out and across it.
This applies both to NPCs looking for the player and to the player's screen.

On the player's screen, NPCs and pickups fade by how much of their body is in grass:
fully visible up to the first `ScreenCoverFade` threshold, not drawn from the second, and
fading between. Anything the player has a line of sight to is drawn fully. The screen eases
towards that target over `CoverFadeSeconds`, so an NPC revealed when the player steps into
its patch fades in rather than popping. `updateCoverFades` keeps this `screenVisibility`
and `buildRenderScene` draws it. NPCs still see the
player by line of sight alone. The debug overlay shows everything.

The player's own sprite shows whether the world can see them. Their `screenVisibility`
eases towards their own cover fade, raised to fully exposed while any NPC saw them this
update or for `ShotRevealSeconds` after they fire. "Saw them" is read straight from the
`targetVisible` flag that `updateNpcSenses` stamps on each brain, so there is one rule
for who sees the player, decided once per tick by gameplay, and the screen only reports it. The renderer draws the player shaded by
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
loads shared definitions from `tiles.json` beside
`levels.json`. Each level's `tileLegend` maps its one-character map symbols to catalogue
names; there is no default, so a level says what every symbol it uses means.
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
clearance. Platformer actors also need ground support, while flying actors do not.
Invalid content fails during loading with the level ID, actor ID, and invalid
location.

## NPC behaviour

### Sensing and memory

An NPC detects the living player when the player is within its notice distance and a
tile segment cast finds clear line of sight. It stores the player's ID and last seen
feet. When sight is lost, a configurable timer lets it continue toward the remembered
position before forgetting the target. Firing gives the player away without making
them visible: every opponent NPC within notice distance hears the shot through any
tiles, remembers the player's feet at that moment, and starts the same timer. The
weapon stamps each shot with the world clock, and senses hear the shot whose stamp is
one update old.

Ground NPCs chase a standable destination using their own collider size. If the
last-seen feet cell is not standable (for example, during a jump or just past a
platform edge), `findPlatformerChaseCell` selects the nearest standable feet position.
Equal-distance candidates use row, then column order. Pathfinding still determines
whether that destination is reachable; there is no fallback to a different destination
if it is disconnected. This selection never reads the hidden player's current position
or moves either actor directly. Patrol endpoints remain exact, and flyers continue
to use the last-seen feet cell.

Sensing only records observations. It does not decide whether to patrol, chase, bite,
or shoot. This keeps perception and decisions separately testable.

### Explicit state machine

`NpcState` is an enum and `updateNpcBehaviour` uses explicit branching. The main states
are Idle, Patrol, Chase, and Bite. Ranged NPCs use the same pursuit state but request
their ranged primary attack when the target is visible and in range.

Behaviour chooses a destination or attack and writes intentions. It does not move the
body directly. If a ground NPC reaches an awkward platform edge and loses its path,
navigation can recover to a supported cell before repathing; regression tests cover
this case.

## Navigation

Navigation is intentionally the most advanced subsystem. It separates a generic
lowest-cost search from movement-specific neighbour policies.

`path_search` accepts connections with positive costs and an optional heuristic.
Supplying zero produces Dijkstra-style lowest-cost search. A policy hands the search
each cell's connections by visiting them where they are, each with the cost the search
should charge, so a policy reading a cache need not copy them out; only the
connections the search follows are copied, into the path. A search is told the size of
its grid and keeps a slot per cell, so a connection's destination is found without
hashing or scanning. Flying navigation uses
ordinary walkable grid neighbours and can use Manhattan distance. Platformer
navigation uses fixed simulation ticks as the common connection cost and a conservative
tick estimate as its heuristic.

### Flying paths

Flying navigation treats cells that allow movement as nodes connected in four directions.
The path follower steers toward successive cell destinations while collision keeps the
body outside platforms. Arrival uses body-aware tolerances so a smaller bat does not
remain stuck against a platform corner.

### Platformer paths

Platformer nodes are standable grid positions. Connections have an action: Walk, Fall,
or Jump. Jump and fall connections also store an `InputProgram`, a sequence of
intentions and tick counts that can be replayed by the path follower.

Neighbour generation reuses the real platformer movement and collision functions at
the step the caller passes in, which the NPC system takes from the tick it is running,
so a predicted jump and the real one run the same physics; the debug overlay replays
recorded jumps at the application's step for the same reason. A simulated jump is
accepted only when it lands on another standable cell. Walk connections scan continuously walkable cells and include braking at their
destination. Raw connection durations are measured in simulation ticks. The
high-level platformer search can add a configurable jump-start penalty, also expressed
in ticks, so a marginal shortcut does not make a grounded NPC hop unnecessarily.
Setting that penalty to zero selects strictly by simulated travel time.

A cell's connections depend only on the map, the cell, the body's size, its movement
configuration and the step, so simulating them once is enough while the map stands. When
a projectile breaks a tile, the cache drops only what the break can have changed. Each
cell's connections are kept with a footprint, the rectangle of cells their simulation
swept or read, grown a tile all round for the tiles collision and support look at beside
the body; a broken tile inside a footprint drops that cell, along with any reachable set
that held it and every remembered path, since a new opening can make a cheaper route
anywhere. The map logs the cells it breaks, and the cache syncs with the log whenever it
is read with the map to hand, so no other system has to tell it. The cells a break drops
join a refill queue per body, and every simulation step begins with a refill phase that
simulates and keeps them again, a cell at a time, until a budget of movement ticks is
spent, so a break costs a little on each of the steps that follow instead of everything
on one. A search that expands a cell still in the queue does not simulate it: it moves
the cell to the front of the queue, searches on without its connections, and if it finds
no path that way reports itself deferred and keeps nothing, so the NPC asks again next
step rather than waiting out its cooldown; a path it does find is still a path. An NPC
also plans again after any break, since its path may have run through the broken tile.
`PlatformerConnectionCache` in `navigation/connection_cache`
keeps the connections leaving each cell, grouped by the body they were simulated for.
A search handed a cache reads each expanded cell's connections where the cache keeps
them, simulating and keeping them first when it does not yet; a search without one
simulates every cell, as the tests of the policies do. The `World` owns the cache for the map it is
simulated with, since the world is replaced with its level, and the NPC system hands it
to every platformer search. The game fills it when a level starts: `warmNpcNavigation`
keeps every cell of the map for each platformer NPC body in the world, at the step the
game is simulated with, which takes a few milliseconds per shipped level in a release
build, so the first chase simulates nothing during play. A search that fails has expanded every cell its start
leads to, and the cache keeps that set too, per start and body, so a later search from
there to a goal outside it returns no path without expanding anything; an NPC that can
see a player it cannot reach retries every quarter second at no cost. A search that
succeeds is kept as well, by start, goal, body and jump penalty, since the connections
never change and so neither does the cheapest route: a patrol searches each of its
legs once, and a chase back to a cell it has been to costs a lookup. The profile counts
the searches answered this way beside the ones that ran. The profile
counts the cells a search reused beside the cells it expanded, so the frame panel shows
the simulated ticks fall to nothing once the level's reachable cells have been found.

Path following never teleports an actor or writes its velocity. It emits intentions,
and the ordinary actor movement system performs the motion. End-to-end tests replay
generated input programs through the real simulation so navigation cannot quietly
drift away from runtime movement.

One limitation is deliberate. Ground navigation naturally uses actor feet, while a flying
actor is easier to reason about from its centre. The current API keeps feet-based
destinations for both so the navigation data model stays uniform, at the cost of a slightly
awkward fit for flying actors. An explicitly named navigation anchor is
[future work](FUTURE_WORK.md).

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
The final exit shows completion text and R creates a fresh copy of the catalogue's start
level.

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
region is the catalogue's `tileSize` square and `tiles.json` gives only where it starts.
Every other sprite in the same atlas chooses its world size independently.

Matching sizes are assigned explicitly; the engine does not assume a sprite and body
are equal. Actor sprites are normally positioned from the body's feet, which lets a
tall image use a smaller collider. The bat additionally uses a centred sprite anchor so
its smaller collider matches the creature in the middle of its frame.

### Animation

Clips are authored in `animations.json`; [CONTENT.md](CONTENT.md#animation-sets) covers
the format. The catalogue loads before actors and stays unchanged for the session, and
composition creates a fresh animator for each actor. JSON defines clips, not selection
rules.

There is no animation state machine. A priority function selects a clip from life,
attack, grounded, and velocity state; death has highest priority, then attack.

Each animated actor has an `Animator` with its current animation, elapsed time, and an
`AnimationSet`. Each character therefore owns its clip definitions and can use different
atlas positions and frame counts. `updateWorldAnimations` calls `updateActorAnimations`
to select and advance clips after simulation and write the selected source region to
the actor's `Sprite`. Pickup bobbing, hit flashes, death fading, and projectile bursts
are calculated during scene construction from gameplay state and timers; they do not
all require animation clips.

The supplied atlas is 160 by 248 pixels. The example character clips use fixed 32 by
24 source frames and separate animation sets for the player, zombie, bat, and zombie
soldier. Artwork sources and atlas tooling live outside this repository; what is here is
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
F1 toggles the debug overlay. The overlay can show actor details, sprite and collision
bounds, pickups, projectiles, bite hitboxes, camera bounds, dead zone, NPC sensing,
navigation paths, and the connection cache's cells for one platformer NPC body, N moving
to the next: filled with their connection count while kept, outlined while missing, which
after a break is what the break dropped and the refill has not reached yet, with the
cache's totals under the actor text, including the cells waiting for the refill and the
cells dropped and kept so far. With
the overlay open, a tile under the cursor that can break is labelled, and B breaks it as
a shot would, so what a break does to the cache can be tried without one. For the cell
under the cursor it also outlines the footprint the cell's simulation swept, which is
why a break inside it drops the cell, draws each connection to where it lands with jumps
and falls along their replayed arcs, and shades the cells a failed search found
reachable from it. Debug data is built separately from its ImGui presentation so it can
be tested without a window.

The overlay also shows a frame panel, drawn by `app/debug/frame_profile_ui`. The
application times each frame with a `Stopwatch` from `timing/stopwatch`, how many fixed
steps it ran, and how long simulation, scene building, rendering, and the interface
took, and records them in a `FrameHistory` from `timing/frame_profile`.
The panel is one ImPlot plot over the recent frames with two vertical axes: frame time against the 60 Hz budget line on the left, and the simulation's phases on the right, stacked by category (NPC, Movement, Combat, World) on a fixed scale so a slow frame shows as a spike that may leave the top rather than rescaling the stack. Hiding a category in the legend restacks the rest. Under the plot it prints the latest breakdown, the average, the worst frame, and every phase under its category as an average cost per simulation step over the history, since one frame's numbers change too fast to read. The panel's window is invisible to the mouse, so clicks over it reach the game like the rest of the overlay; its legend and its plot are small windows of their own and the two places a click lands. A press on the plot picks the frame under the cursor and holding the button scrubs along the frames: a `FrameSelection` from `app/debug/frame_selection` keeps a copy of the history as it was, the plot holds still with the picked frame marked, and the summary shows that frame's own costs, its phases in milliseconds rather than per step and listed by cost, the dearest category first and each category's dearest phase first, until the picked frame is clicked again. The selection is data without ImGui, so what a press or a drag picks and what it keeps are tested. When the overlay is open, the simulation step is also handed the profile and charges each of its phases to it under a category and a short name, and the NPC system times each path search as a phase of its own, so the behaviour phase keeps only its own time, and counts the searches it ran, how many of those waited for a refill, the cells they expanded, how many of those the connection cache already held, the movement ticks they simulated, and the ticks the refill phase simulated; the stack shows which category widened in a slow frame. `Stopwatch` is the one place the engine reads a clock; `timePhase` in `timing/frame_profile` times with it, and only when asked; with no profile nothing is timed, and tests build profiles by hand. Timings are only
meaningful from a release build.

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

A first small feature can extend the existing platformer subject directly. If a game
adds several optional abilities, use the component-and-modifier direction described
under [Optional movement abilities](FUTURE_WORK.md#optional-movement-abilities)
instead of filling
`PlatformerMovement` with unrelated flags.

### Adding an NPC state

`NpcState` represents what an NPC is doing now. To add a state such as Search, Guard,
Retreat, or Recover:

1. Add the state to the enum and give its entry and exit conditions explicit branches
   in the NPC system.
2. Reset state-local timing in the same place as the other transitions.
3. Let the state choose a destination, facing, or attack intention.
4. Continue to move and attack through `InputIntentions`; NPC decision code should not
   write body position or bypass combat systems.
5. Test entry, sustained behaviour, exit, and the most important interaction with
   sensing or target memory.
6. Add the state name to the debug presentation so it can be inspected while playing.

Keep the enum and explicit state branches while the number of states is small. A
behaviour tree, virtual brain hierarchy, or callback registry would make transitions
and state ownership harder to follow without solving a current requirement.

### Creating a new enemy

First decide whether the enemy is only a differently configured existing role. If it
uses the same capabilities, add a named definition in `actors.json`. A
genuinely new example enemy normally involves:

1. a symbolic actor definition in `actors.json`;
2. a matching `definition` in the level's `actors` array or an object legend entry
   with `"type": "actor"`;
3. an actor composed from only the movement, sensing, path-following, health, attack,
   and presentation components it needs;
4. an animation set and atlas regions in the example game;
5. valid spawn and patrol data in a level JSON file;
6. focused tests for its new decision rule, with broader simulation coverage only for
   interactions between systems.

Species, capabilities, and decisions are separate concerns. Artwork does not determine
the brain, and possessing a ranged weapon does not require a `Shooter` subclass. The
future [NPC tactics](FUTURE_WORK.md#npc-tactics) section describes how to introduce
multiple reusable
decision policies once the game contains a real second policy.

### Choosing the layer

| Change | Primary location |
| --- | --- |
| Input binding or mouse conversion | `app/application.cpp` |
| Movement or collision rule | `src/movement` or `src/physics` |
| NPC perception or decision | `src/npc` |
| Generic search or movement-specific neighbours | `src/navigation` |
| Damage, attacks, or projectiles | `src/combat` |
| Animation definitions | `assets/animations.json` |
| Content loading and validation | `app/content` |
| Playable level composition and session flow | `app/game` |
| Actor, tile, item, pickup, and exit definitions; level geometry and placements | `assets` |
| HUD or debugging presentation | `app/ui` or `app/debug` |

When a feature crosses layers, keep its rule in the simulation and pass plain state to
presentation. Add the smallest test at the layer that owns the rule before adding an
end-to-end test.

## Error handling and validation

Validation has three boundaries:

1. **JSON shape:** loaders check types, integer ranges, required fields, and reject unknown
   fields to catch misspellings. This includes catalogues, level placements, and legend
   templates. `content_json` provides shared file-reading and shape-checking helpers.
   `content_diagnostics` builds the field paths and raises the errors, and carries no
   JSON dependency so the C++ validators can use it too.
   `readInteger`, `readNumber`, `readBoolean`, `readText`, and `readVector` read
   required fields. Their `readOptional...` counterparts leave C++ defaults unchanged
   only when a field is absent; present but invalid values are errors. Both use the
   same `json...` value checks and accept a source filename and field path.
2. **Application content:** plain C++ validators check authoring rules.
   [`content_validation.cpp`](../app/content/content_validation.cpp) covers legends, map rows,
   placement counts, quantities, and exit settings. Actor, item, pickup, and exit catalogue
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
- lowest-cost search, heuristics, flying paths, standability, falls, and replayed jump
  programs;
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

| Change | Test starting point |
| --- | --- |
| Content parsing or definition validation | `tests/app/test_*_catalog.cpp`, `test_level_data.cpp`, `test_content_validation.cpp` |
| Composing catalogue entries into levels | `tests/app/test_level_composition.cpp` |
| Carrying player state between levels | `tests/app/test_level_transition.cpp` |
| Core pickup collection or exit rules | `tests/world/test_level_objects.cpp` |
| Behaviour involving multiple systems | `tests/world/test_world_simulation.cpp` |
| Visual state converted to draw commands | `tests/render/test_render_scene.cpp` |

Use small independent data in tests rather than asserting the example campaign's
enemy count, item values, or inventory capacity. Its own checks should test validity,
so you can change content without rewriting unrelated tests.

OpenGL and ImGui integration remain a manual run; automated graphics-context tests are
avoided. [README.md](../README.md#continuous-integration) lists what CI checks.
