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
- health, death, respawning, pickups, inventory, and two connected levels;
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
UI, and rendering. `ExampleGame` owns the current `TileMap`, `World`, and camera
controller. It translates application input into game input, invokes the engine, builds
the render scene, and replaces the world during a level transition.

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

1. Update NPC sensing and target memory.
2. Update NPC decisions, destinations, paths, and intentions.
3. Move every actor and resolve tile collision.
4. Advance attacks and evaluate active bite hitboxes.
5. Move projectiles and find their earliest collision.
6. Apply damage and advance actor life cycles.
7. Detect automatic pickups.
8. Apply queued world requests.
9. Check the level exit.

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

`World` owns actors, projectiles, pickups, item definitions, and the current exit. An
actor has a typed, monotonically increasing ID rather than exposing its vector index:

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

After level data is loaded and composed into runtime objects, `validateLevelActors`
checks it against the map.
Every actor spawn, the player's stored respawn, and every patrol endpoint need body
clearance. Platformer actors also need ground support, while flying actors do not.
Invalid content fails during loading with the level number, actor ID, and invalid
location.

## NPC behaviour

### Sensing and memory

An NPC detects the living player when the player is within its notice distance and a
tile segment cast finds clear line of sight. It stores the player's ID and last seen
feet. When sight is lost, a configurable timer lets it continue toward the remembered
position before forgetting the target.

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
destination. Because every action cost is measured in simulation ticks, the search can
compare walking and jumping without mixing unrelated distance and time units.

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
removed. Owner and team prevent hitting the shooter or allies.

Damage is queued rather than applied while attacks and projectiles are being traversed.
`updateLifeState` consumes the requests, changes an actor from Alive to Dying when
health reaches zero, and advances its short death timer. Dying actors cannot decide,
accept gameplay input, attack, or take another hit, but gravity and collision continue.
At the end of the timer an NPC is removed; the player is respawned at its stored feet
position with restored health and movement runtime state.

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
contains coins, health potions, and a key. Inventory persists through player death.

An exit can require an item and optionally consume it. Exit completion is latched so a
requirement cannot be consumed twice. The simulation reports completion;
`ExampleLevel` keeps a level number, map, populated world, and player spawn together so
callers cannot accidentally combine data from different levels. `ExampleGame` replaces
that value at a transition, restores player health and inventory, and resets the camera.
Velocities, projectiles, NPC state, and old actor IDs do not cross the level boundary.
The final exit shows completion text and R creates a fresh Level 1 game.

### Data-driven level boundary

Files under `assets/levels` define tile rows, the player spawn, known NPC placements
and patrol endpoints, pickups, and the exit. They select and place concepts rather than
defining new engine behaviour. For example, `"type": "zombie"` selects the zombie
factory in `example_content.cpp`; the JSON does not list arbitrary `Actor` components.

Map-aligned actor and patrol placement normally uses integer cells such as
`"spawnCell": [28, 12]`. The loader converts a cell to its world-space feet position.
An explicitly named `spawnFeet` remains available for intentional off-grid placement;
the loader requires exactly one form. Actors, pickups, and exits share this convention,
and the same rule applies to each patrol endpoint. Feet are a stable bottom-centre
reference point and do not imply that an object must stand on the ground.

Level data does not specify actor, pickup, or exit bounds. The C++ example-content
factories own those collision sizes and create each runtime AABB around its loaded feet
position. Visual anchoring remains an independent sprite concern.

`example_level_data.cpp` is the only code that includes nlohmann/json. It parses a file
into plain `ExampleLevelData`, reports invalid fields with their content path, and maps
stable names such as `zombie_soldier` and `health_potion` to C++ values. The composition
step then creates the existing `TileMap`, `World`, actors, pickups, and exit. Existing
constructor and level validation remains authoritative.

Runtime-only state is never loaded: actor IDs, velocities, current paths, attack timers,
and NPC decisions are created fresh whenever a level starts. Texture IDs and atlas
regions also remain C++ application resources rather than values in the level files.

The inventory UI is an example presentation, not an engine rule. It derives its rows
from the configured slot count, uses at most three columns, pauses simulation while
open, and emits item use requests instead of changing the world directly.

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
