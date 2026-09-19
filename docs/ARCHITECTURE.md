# Simple Platformer Architecture

This document explains the architecture that exists in the repository now: its main
boundaries, data model, runtime flow, and the reasons behind them.

If this is your first time in the project, follow [START_HERE.md](START_HERE.md) before
reading this document from top to bottom.

Use this as a reference when working on a particular feature:

**Orientation**

- [Purpose and scope](#purpose-and-scope): what the repository is and is not.
- [Project shape](#project-shape): targets, folders, and the dependency boundary.
- [Runtime flow](#runtime-flow): the fixed step and the order systems run in.
- [Coordinates and time](#coordinates-and-time): axes, feet positions, and the shared clock.

**The data model**

- [World ownership and identity](#world-ownership-and-identity): what the world owns.
- [Actor composition](#actor-composition): how capabilities fit together.

**Gameplay systems**

- [Input and movement](#input-and-movement): intentions, platformer and flying movement.
- [Tile map, collision, and validation](#tile-map-collision-and-validation): terrain and sweeps.
- [NPC behaviour](#npc-behaviour): sensing, memory, and the explicit state machine.
- [Navigation](#navigation): path search, following, and simulated jumps.
- [Combat, projectiles, and life cycle](#combat-projectiles-and-life-cycle): attacks and death.
- [Inventory, pickups, and levels](#inventory-pickups-and-levels): the level loop and the
  [data-driven boundary](#data-driven-level-boundary). [CONTENT.md](CONTENT.md) is the
  file-by-file authoring reference.

**Presentation and practice**

- [Presentation](#presentation): animation, rendering, camera, and UI.
- [Extension recipes](#extension-recipes-for-project-work): where to make a gameplay change.
- [Error handling and validation](#error-handling-and-validation): which layer rejects what.
- [Testing and quality checks](#testing-and-quality-checks): how to verify it.

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
- `simple_platformer_tests` contains Catch2 tests, primarily against the core.

This boundary is important for teaching and testing. A test can construct a `World`,
run movement or a complete simulation tick, and inspect the result without needing a
window or graphics context.

All third-party source is vendored under `external/` so the project builds offline and
everyone works from the same releases. The current dependencies include GLFW, glad, GLM, ImGui,
Catch2, stb image loading, and nlohmann/json.

### Application folders

The application code is grouped by responsibility:

- `app/game` coordinates the game session and composes playable levels;
- `app/content` contains content definitions, JSON loaders, catalogues, and validators;
- `app/ui` contains player-facing HUD, inventory, and completion UI;
- `app/debug` builds and presents optional debugging information;
- `app/graphics` contains display-viewport conversion and OpenGL sprite submission.

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
2. Update NPC sensing and target memory.
3. Update NPC decisions, destinations, paths, and intentions.
4. Move every actor and resolve tile collision.
5. Advance attacks and evaluate active bite hitboxes.
6. Move projectiles and find their earliest collision.
7. Advance existing projectile bursts and queue expired bursts for removal.
8. Apply damage and advance actor life cycles.
9. Detect automatic pickups.
10. Apply queued world requests.
11. Check the level exit.

The player intentions are written before this sequence. Camera and actor animation are
updated afterward on ordinary gameplay ticks because they present the resulting state.
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
- Tiles are 16 by 16 world pixels.
- Window output is an integer-scaled internal image with letterboxing when required.
- `World` owns elapsed simulation time. It advances once per fixed simulation update and
  provides a shared clock for effects that do not need their own resettable timer.
  Actors store damage timestamps against this clock, while rendering decides how recent
  damage should look.

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
definitions, and the current exit. An actor has a typed, monotonically increasing ID
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
Aim is independent of travel direction. Facing is still left or right for sprite
flipping and follows horizontal aim when appropriate.

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

Movement produces velocity. The collision module moves the body and returns contacts;
movement then observes those contacts. This direction keeps platformer rules separate
from tile collision and avoids a general ability framework.

### Flying movement

Flying movement normalises a nonzero two-dimensional intention, multiplies it by the
configured speed, and uses the same tile collision function. It has no gravity,
jumping, or acceleration state.

## Tile map, collision, and validation

`TileMap` stores a rectangular row-major vector of integer tile IDs. Tile zero is
empty. Each nonzero tile definition supplies a sprite region, `blocksMovement`, and
`blocksSight`. The same map layer supports rendering, collision, and sensing.
Glass blocks movement and projectiles but allows sight. Grass allows movement and
projectiles but blocks sight rays. These are static tiles: glass does not yet break,
and grass does not hide actors, pickups, or exits from the player's screen.

Examples and tests construct maps from ASCII strings using `.` for empty and `#` for
solid by default. The example loads shared definitions from `tiles.json` beside
`levels.json`. An optional `tileLegend` in each level maps one-character symbols to
catalogue names; without it, `.` means `empty` and `#` means `stone`.
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
position before forgetting the target.

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
Supplying zero produces Dijkstra-style lowest-cost search. Flying navigation uses
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
the fixed step. A simulated jump is accepted only when it lands on another standable
cell. Walk connections scan continuously walkable cells and include braking at their
destination. Raw connection durations are measured in simulation ticks. The
high-level platformer search can add a configurable jump-start penalty, also expressed
in ticks, so a marginal shortcut does not make a grounded NPC hop unnecessarily.
Setting that penalty to zero selects strictly by simulated travel time.

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

The living player automatically collects pickups on strict body overlap. NPCs do not.
A pickup that cannot fit completely remains with its uncollected quantity. The example
contains coins, health potions, and a key. Pickup sprites use the shared World clock and
a position-based phase offset to bob without moving their collection bounds. Inventory
persists through player death.

An exit can require an item and optionally consume it. Exit completion is latched so a
requirement cannot be consumed twice. The simulation reports completion;
`GameLevel` groups a level's data so callers cannot accidentally combine parts of
different levels. `Game` replaces that value at a transition, carries over the player's current health and inventory,
and resets the camera.
Velocities, projectiles, NPC state, and old actor IDs do not cross the level boundary.
The final exit shows completion text and R creates a fresh copy of the catalogue's start
level.

### Example campaign

The three levels in `assets` use the same movement and combat systems with
different layouts. Each exit requires and consumes one key. Coins and health potions
are optional rewards, not exit requirements.

1. **Introduction:** low obstacles and patrolling zombies lead toward a key and an
   exit to the right, with optional rewards on raised platforms.
2. **Route choice:** the upper route crosses platform gaps guarded by bats. The lower
   route passes a zombie soldier, with solid cover breaking its line of sight. Both
   routes meet at the key platform before the exit.
3. **Key hunt and return:** the exit is near the starting point. A stepped climb past
   a zombie, soldier, and bat reaches the key high on the right. Dropping off the right
   side leads to a lower return route with cover, enemies, and optional supplies.

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
F1 toggles the debug overlay. The overlay can show actor details, sprite and collision
bounds, pickups, projectiles, bite hitboxes, camera bounds, dead zone, NPC sensing, and
navigation paths. Debug data is built separately from its ImGui presentation so it can
be tested without a window.

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
