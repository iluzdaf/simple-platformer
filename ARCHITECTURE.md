# Simple Platformer Architecture

This document explains the architecture that exists in the repository now: its main
boundaries, data model, runtime flow, and the reasons behind them. It is a reference,
not an implementation schedule.

If this is your first time in the project, follow [START_HERE.md](START_HERE.md) before
reading this document from top to bottom.

## Purpose and scope

Simple Platformer is a small C++17 teaching engine with a complete example game. It
keeps the code explicit enough to trace in a debugger and separates gameplay rules
from graphics so the major paths can be tested without opening a window.

The current example includes:

- responsive platformer movement with variable-height jumping, coyote time, and jump
  buffering;
- arbitrary-sized AABB bodies colliding with a solid tile grid;
- scrolling maps and a dead-zone camera;
- actors assembled by composition;
- player and NPC control through the same `InputIntentions`;
- enum-and-switch NPC state machines, sensing, and target memory;
- flying and platformer pathfinding;
- 360-degree projectiles and a timed bite attack;
- health, death, respawning, pickups, inventory, and three connected levels;
- sprite animation, an ImGui HUD, and an optional debug overlay.

The project deliberately does not try to provide slopes, one-way or moving platforms,
dynamic rigid-body physics, actor pushing, multiplayer, scripting, save games, an
editor, an animation graph, a general ECS, or advanced projectile modifiers such as
homing and piercing. OpenGL submission is checked manually rather than with automated
graphics integration tests.

## Project shape

### Targets and dependency boundary

The project has three main CMake targets:

- `simple_platformer_core` contains simulation and render-scene construction. It has
  no dependency on GLFW, OpenGL, or ImGui.
- `simple_platformer` contains the executable, window, input adapter, OpenGL renderer,
  and ImGui presentation.
- `simple_platformer_tests` contains Catch2 tests, primarily against the core.

This boundary is important for teaching and testing. A test can construct a `World`,
run movement or a complete simulation tick, and inspect the result without needing a
window or graphics context.

All third-party source is vendored under `external/` so the project builds offline and
students use the same releases. The current dependencies include GLFW, GLM, ImGui,
Catch2, stb image loading, and nlohmann/json.

### Application folders

The application code is grouped by responsibility:

- `app/game` contains the example game's orchestration, level loader, actor factories,
  items, and animation sets;
- `app/ui` contains player-facing HUD, inventory, and completion UI;
- `app/debug` builds and presents optional debugging information;
- `app/graphics` contains display-viewport conversion and OpenGL sprite submission.

`application.cpp` owns the outer loop: window events, input collection, fixed updates,
UI, and rendering. `ExampleGame` owns the current `GameLevel` and camera controller.
`GameLevel` keeps the level ID, `TileMap`, `World`, and player spawn together.
`ExampleGame` translates application input into game input, invokes the engine, builds
the render scene, and replaces the level during a transition.

Example-specific content is kept out of general engine systems. Level geometry and
placements live in `assets/levels`; the loader, actor factories, item definitions, and
animation clips live under `app/game`.

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

The application gathers input and gives the player's intentions to `ExampleGame`.
Simulation advances at a fixed 60 Hz (`1.0F / 60.0F`); rendering runs at the available
frame rate. Frame time is clamped before it enters the fixed-step accumulator so a
breakpoint or stall does not cause an excessive catch-up.

`updateWorldSimulation` is the authoritative gameplay order:

1. Advance the World's shared simulation clock.
2. Update NPC sensing and target memory.
3. Update NPC decisions, destinations, paths, and intentions.
4. Move every actor and resolve tile collision.
5. Advance attacks and evaluate active bite hitboxes.
6. Move projectiles and find their earliest collision.
7. Apply damage and advance actor life cycles.
8. Detect automatic pickups.
9. Apply queued world requests.
10. Check the level exit.

The player intentions are written before this sequence. Camera and actor animation are
updated afterward because they present the resulting gameplay state and do not affect
the simulation.

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
type: call the same factory with different spawn and patrol data. A bat can use a
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

Flying movement normalizes a nonzero two-dimensional intention, multiplies it by the
configured speed, and uses the same tile collision function. It has no gravity,
jumping, or acceleration state.

## Tile map, collision, and validation

`TileMap` stores a rectangular row-major vector of integer tile IDs. Tile zero is
empty. Each nonzero tile definition supplies a sprite region and whether the tile is
solid. The same map layer supports rendering and collision.

Examples and tests construct maps from ASCII strings using `.` for empty and `#` for
solid. The example reads those rows from its level JSON. Actors, pickups, spawns, and
exits are separate level data, not special tile IDs.

Collision moves an arbitrary-sized AABB along X, resolves it against nearby full-tile
AABBs, then repeats along Y. The result reports left, right, ground, and ceiling
contacts. Actors do not physically collide with or push one another. The left, right,
and bottom map boundaries block movement; the top remains open.

`segmentCast` finds the first point where a line enters one AABB.
`segmentCastSolidTiles` applies that operation to the relevant part of a tile map and
can account for a moving box size. Projectile collision and NPC sight share this tile
cast while keeping their own gameplay rules.

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

Flying navigation treats non-solid grid cells as nodes connected in four directions.
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
position, chooses the earliest solid-tile or eligible-actor hit, queues damage, and is
removed. Owner and team prevent hitting the shooter or allies. When a projectile ends,
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
`GameLevel` keeps a level ID, map, populated world, and player spawn together so
callers cannot accidentally combine data from different levels. `ExampleGame` replaces
that value at a transition, restores player health and inventory, and resets the camera.
Velocities, projectiles, NPC state, and old actor IDs do not cross the level boundary.
The final exit shows completion text and R creates a fresh copy of the catalog's start
level.

### Example campaign

The three levels in `assets/levels` use the same movement and combat systems with
different layouts. Each exit requires and consumes one key. Coins and health potions
are optional rewards, not exit requirements.

1. **Introduction:** low obstacles lead past one patrolling zombie to a raised key.
   A health potion sits before the climb, and the exit is beyond it to the right.
2. **Route choice:** the upper route crosses platform gaps guarded by bats. The lower
   route passes a zombie soldier, with solid cover breaking its line of sight. Both
   routes meet at the key platform before the exit.
3. **Key hunt and return:** the exit is near the starting point. A stepped climb past
   a zombie, soldier, and bat reaches the key high on the right. Dropping off the right
   side leads to a lower return route with cover, a zombie, and a health potion.

### Data-driven level boundary

The example game loads its levels from `assets/levels`. The files select and place known
game concepts rather than defining new engine behaviour. For example,
`"type": "zombie"` selects the zombie factory in `example_content.cpp`; the JSON does
not list arbitrary `Actor` components.

#### Level catalog

`assets/levels/levels.json` selects the starting level and assigns stable numeric level
IDs to files:

```json
{
  "startLevel": 1,
  "levels": [
    {"number": 1, "file": "level_1.json"},
    {"number": 2, "file": "level_2.json"},
    {"number": 3, "file": "level_3.json"}
  ]
}
```

- `number` is a positive, unique level ID.
- `file` is a path relative to the catalog's directory.
- `startLevel` names one of the catalog entries.
- An exit's `nextLevel` refers to a level ID in the catalog.

Each catalog entry assigns a level ID to a level file. The referenced file contains that
level's map and object placements. Students can rename, add, or remove level files by
updating the catalog without changing C++.

#### Level files

A minimal level looks like this:

```json
{
  "map": [
    "........",
    "........",
    "########"
  ],
  "playerSpawnCell": [1, 1],
  "actors": [],
  "pickups": [],
  "exit": {
    "spawnCell": [6, 1]
  }
}
```

Every level requires `map`, one player spawn, `actors`, `pickups`, and `exit`. The actor
and pickup arrays may be empty. An exit without `nextLevel` completes the game.

#### Maps and positions

Map rows have the same non-zero length. A `.` is an empty tile and a `#` is a solid
tile. World coordinates begin at the top-left: positive X points right and positive Y
points down. A cell position is `[column, row]`, also counted from the top-left.

Actors, pickups, exits, and patrol endpoints support two placement forms:

- `spawnCell` places the object's feet at the bottom-centre of a tile cell.
- `spawnFeet` supplies that bottom-centre position directly in world pixels.

The player spawn uses the corresponding names `playerSpawnCell` and `playerSpawnFeet`.
Each placement uses exactly one form. Patrol endpoints use `firstCell` or `firstFeet`,
and `secondCell` or `secondFeet`. Feet provide a stable bottom-centre reference point
and do not imply that an object must stand on the ground.

#### Actors

An actor requires `type` and one spawn placement. The supported example types are
`zombie`, `bat`, and `zombie_soldier`. A patrol is optional:

```json
{
  "type": "zombie",
  "spawnCell": [5, 8],
  "patrol": {
    "firstCell": [5, 8],
    "secondCell": [10, 8]
  }
}
```

The C++ factories in `app/game/example_content.cpp` decide each actor's body size,
movement, combat behaviour, sprite, and animation set.

#### Pickups

A pickup requires a supported item name, a positive quantity, and one spawn placement:

```json
{
  "item": "health_potion",
  "quantity": 2,
  "spawnCell": [4, 8]
}
```

The supported example items are `coin`, `health_potion`, and `key`. Their definitions
and effects remain in C++.

#### Exits

An exit requires one spawn placement. It may also contain:

- `requirement`, with an item and positive quantity;
- `consumeItem`, which defaults to `false`;
- `nextLevel`, which refers to a level ID in `levels.json`.

```json
{
  "spawnCell": [18, 8],
  "requirement": {
    "item": "key",
    "quantity": 1
  },
  "consumeItem": true,
  "nextLevel": 2
}
```

#### Loading and composition

Level data does not specify actor, pickup, or exit bounds. The C++ example-content
factories own those collision sizes and create each runtime AABB around its loaded feet
position. All example pickups use the same 16-by-16 collision bounds. Their item sprites
remain independent, just like actor sprites and bodies.

The JSON dependency stays at the application content boundary.
`level_catalog.cpp` validates the catalog, and `example_level_data.cpp` parses a
level into plain `ExampleLevelData`, reports invalid fields with their content path, and
maps stable names such as `zombie_soldier` and `health_potion` to C++ values. The
composition step then creates the existing `TileMap`, `World`, actors, pickups, and
exit. Existing construction and level validation remain authoritative.

Parser tests use JSON strings, while transition tests use small files under
`tests/fixtures`. One generic content check loads every entry in the editable catalog;
it does not assume particular filenames, a fixed level count, or specific NPCs.

Runtime-only state is never loaded: actor IDs, velocities, current paths, attack timers,
and NPC decisions are created fresh whenever a level starts. Texture IDs and atlas
regions also remain C++ application resources rather than values in the level files.

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

Animations use named clips: Idle, Move, Jump, Fall, Attack, and Death. There is no
animation state machine. A priority function selects a name from life, attack,
grounded, and velocity state; death has highest priority, then attack.

Each animated actor has an `Animator` with its current animation, elapsed time, and an
`AnimationSet`. Each character therefore owns its clip definitions and can use different
atlas positions and frame counts. `updateActorAnimations` selects and advances clips
after simulation and writes the selected source region to the actor's `Sprite`.

The supplied atlas is 160 by 248 pixels. The example character clips use fixed 32 by
24 source frames and separate animation sets for the player, zombie, bat, and zombie
soldier. Artwork sources and atlas tooling live outside this student repository; this
repository contains the finished runtime atlas.

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
under [Optional movement abilities](#optional-movement-abilities) instead of filling
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
uses the same capabilities, reuse the existing factory with different level data. A
genuinely new example enemy normally involves:

1. a symbolic actor type in the level-data parser;
2. a factory branch in `app/game/example_content.cpp`;
3. an actor composed from only the movement, sensing, path-following, health, attack,
   and presentation components it needs;
4. an animation set and atlas regions in the example application;
5. valid spawn and patrol data in a level JSON file;
6. focused tests for its new decision rule, with broader simulation coverage only for
   interactions between systems.

Species, capabilities, and decisions are separate concerns. Artwork does not determine
the brain, and possessing a ranged weapon does not require a `Shooter` subclass. The
future [NPC tactics](#npc-tactics) section describes how to introduce multiple reusable
decision policies once the game contains a real second policy.

### Choosing the layer

| Change | Primary location |
| --- | --- |
| Input binding or mouse conversion | `app/application.cpp` |
| Movement or collision rule | `src/movement` or `src/physics` |
| NPC perception or decision | `src/npc` |
| Generic search or movement-specific neighbours | `src/navigation` |
| Damage, attacks, or projectiles | `src/combat` |
| Example actor values, clips, or item definitions | `app/game` |
| Level geometry and placements | `assets/levels` |
| HUD or debugging presentation | `app/ui` or `app/debug` |

When a feature crosses layers, keep its rule in the simulation and pass plain state to
presentation. Add the smallest test at the layer that owns the rule before adding an
end-to-end test.

## Error handling and validation

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

CI builds and tests on macOS with Apple Clang and on Windows through the same generated
Visual Studio solution used by students. A Linux quality job checks formatting,
clang-tidy, and public-header self-containment. OpenGL and ImGui integration remain a
manual run because automated graphics-context tests would add more infrastructure than
teaching value here.

## Future work

Future ideas are collected here so the sections above continue to describe the current
repository.

### Further data-driven content

Level geometry and placement are loaded from validated JSON. Item definitions,
animation clips, and actor composition still live in C++. They can move to separate
validated data files later, but should keep stable symbolic names, preserve the current
runtime structures, and avoid turning level files into arbitrary component or behaviour
scripts.

### Optional movement abilities

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

### NPC tactics

NPC composition should continue to describe what an actor *can do*: platformer or
flying movement, sensing, biting, and shooting. `NpcState` describes what it is doing
right now, such as patrolling, chasing, or attacking. A future tactic can separately
describe how the NPC chooses between those states.

This keeps species, capabilities, and decision-making independent. A zombie and a
soldier can use different artwork and attacks while sharing a guard tactic; a ranged
weapon is a capability rather than a `Shooter` brain. Useful tactics could include:

- `Pursuer`: move as close to the remembered target as the map permits;
- `Guard`: pursue only inside a home region, then return;
- `KeepDistance`: approach or retreat to maintain a useful attack range;
- `Flee`: move away from the target;
- `Patroller`: follow patrol points without pursuing the player.

The first implementation should stay explicit. An enum in `NpcBrain` and a switch in
the NPC system make the available policies and their dispatch visible to students:

```cpp
enum class NpcTactic
{
    Pursuer,
    Guard,
    KeepDistance,
    Flee,
    Patroller
};

struct NpcBrain
{
    NpcTactic tactic = NpcTactic::Pursuer;
    NpcState state = NpcState::Idle;
    // Perception memory and tactic-specific state.
};
```

```cpp
switch (brain.tactic)
{
case NpcTactic::Pursuer:
    updatePursuer(map, world, actor, deltaTime);
    break;
case NpcTactic::Guard:
    updateGuard(map, world, actor, deltaTime);
    break;
case NpcTactic::KeepDistance:
    updateKeepDistance(map, world, actor, deltaTime);
    break;
case NpcTactic::Flee:
    updateFlee(map, world, actor, deltaTime);
    break;
case NpcTactic::Patroller:
    updatePatroller(map, world, actor, deltaTime);
    break;
}
```

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

`NpcTactic` should be introduced only when the game adds a genuinely different second
policy, such as `Guard`. Until then, a single clearly named pursuit implementation is
simpler than an abstraction created for hypothetical behaviours. Virtual brain classes,
callbacks, and a general behaviour-tree framework are not needed for these tactics.

### Movement-specific navigation anchors

Ground navigation naturally uses actor feet, while a flying actor is easier to reason
about from its centre. The current API keeps feet-based destinations for both so the
navigation data model stays uniform. A future revision can introduce an explicitly
named navigation anchor, use feet for platformer actors and centres for flying actors,
and rename patrol point fields so they are neutral about the chosen anchor.

### Mixed-size animation frames

`SpriteRegion` already supports arbitrary source rectangles, but animation playback
currently changes only the region while `Sprite::size` and its anchor remain fixed.
Fixed-size example frames avoid visible stretching.

A future `AnimationFrame` could contain a source region, display size, and pivot or
offset. That would support mixed-size pixel-for-pixel artwork while keeping feet or
another visual anchor stable. Collision bodies must remain independent from animation
frame dimensions.

### Teaching packages

The completed reference can later be divided into staged student exercises with focused
starter code, diagrams, and checkpoints. Those teaching packages should link back to
this current-design reference rather than turning it into a chronological build diary.
