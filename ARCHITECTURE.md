# Simple Platformer Architecture

## Purpose

Simple Platformer is a small C++ teaching engine and complete example game. It keeps
the strongest ideas from Platformer while making every major code path easy for a
beginner to follow and test.

The reference engine comes first. Its implementation is divided into independently
building, tested phases so it can later become a staged student exercise.

## Goals

- Responsive platformer movement: running, variable-height jumping, coyote time,
  jump buffering, gravity, and terminal velocity.
- Simple axis-separated AABB collision against a grid of solid tiles.
- A tile map larger than the viewport and a camera that follows the player through a dead zone.
- Player and NPC actors assembled from the same components.
- Player and NPC control expressed through `InputIntentions`.
- Explicit C++ enum-and-switch finite state machines for NPC decisions.
- Generic lowest-cost grid search with an optional A* heuristic and separate flying
  and platformer navigation policies.
- Platformer paths that include walking, falling, and jumping.
- Straight left/right projectiles for the player and ranged NPCs, plus a deliberate timed bite.
- A small configurable inventory, pickups, health, a key, and a level exit.
- ImGui HUD and a paused inventory example.
- Automated tests for major gameplay and render-scene-building paths.
- A small cross-platform build using C++17, CMake, Apple Clang, and Visual Studio.

## Non-goals for the first version

- Slopes, steps, one-way platforms, or moving platforms
- Dynamic rigid-body physics or actor-to-actor pushing
- Multiplayer
- Scripting or hot reload
- An integrated level editor
- Save games
- Animation graphs or animation state machines
- A general-purpose ECS
- Advanced projectile modifiers such as homing, bouncing, or piercing
- Automated OpenGL integration tests
- Chunked or streaming worlds

## Technology

- C++17
- CMake
- OpenGL, GLFW, and GLM
- ImGui
- Catch2
- `stb_image`
- A fixed vendored release of `nlohmann/json`, introduced with data loading

Dependencies are vendored under `external/` so the project builds offline and every
student uses the same versions. Third-party JSON types stay inside loader `.cpp`
files and never appear in engine interfaces.

## Targets and dependency boundary

The project has three targets:

- `simple_platformer_core`: world simulation and render-scene construction; no
  GLFW, OpenGL, or ImGui dependency.
- `simple_platformer`: window, keyboard, OpenGL renderer, ImGui UI, and main loop.
- `simple_platformer_tests`: Catch2 tests linked primarily to the core.

The core can be built and tested without opening a window or creating a graphics
context. OpenGL submission remains deliberately thin and receives a manual startup
smoke test rather than automated integration tests.

## Coordinates

- Positive X points right.
- Positive Y points down.
- AABBs and tiles use top-left world positions.
- Actor spawns, item spawns, and navigation destinations use feet: the bottom centre
  of an actor's AABB.
- The internal resolution is 320 by 180 pixels with 16-pixel tiles.
- The window scales the internal image by an integer factor.

The conversion is explicit:

```cpp
struct Aabb
{
    glm::vec2 position; // top-left world position
    glm::vec2 size;
};

glm::vec2 feetOf(const Aabb& box);
void placeFeetAt(Aabb& box, glm::vec2 feet);
```

There is no ambiguous `Actor::setPosition`. Code uses `placeFeetAt` for actors and
`body.bounds.position` for the physical top-left position.

## Identity and ownership

`World` owns actors and projectiles in vectors. An actor has a stable, monotonically
increasing typed ID that is not its vector index:

```cpp
struct ActorId
{
    std::uint32_t value = 0;
};
```

Zero is invalid and IDs are not reused during a play session. `World::findActor`
performs a linear search, which is sufficient for the small example and preserves a
simple public model if storage changes later.

Projectiles hold an optional owner `ActorId` and a team. They use the owner to avoid
hitting their shooter. Separate ID types are introduced only if another object needs
stable external identity.

Systems do not retain actor pointers or references across world mutations.

## Composition

Player and NPC are roles applied to the same `Actor` aggregate rather than subclasses:

```cpp
struct Actor
{
    ActorId id;
    Body body;
    InputIntentions intentions;

    std::optional<PlatformerMovement> platformerMovement;
    std::optional<FlyingMovement> flyingMovement;

    LifeState life = LifeState::Alive;
    float deathTimeRemaining = 0.0f;

    std::optional<Sprite> sprite;
    std::optional<Health> health;
    std::optional<RangedWeapon> weapon;
    std::optional<Inventory> inventory;
    std::optional<NpcBrain> brain;
    std::optional<NpcSenses> senses;
    std::optional<Patrol> patrol;
    std::optional<PathFollower> pathFollower;
    std::optional<BiteAttack> bite;
};
```

Keyboard control writes the player's intentions. An NPC brain and path follower write
an NPC's intentions. `ActorSystem` executes both through the same movement, collision,
weapon, and animation-observation pipeline.

An actor has exactly one movement component. The player and ground NPC use
`PlatformerMovement`; the flying NPC uses `FlyingMovement`. Construction validates
that neither zero nor two movement components are present. `ActorSystem` dispatches
explicitly according to the component that exists rather than through an inheritance
hierarchy.

## Input intentions

```cpp
struct InputIntentions
{
    glm::vec2 direction = {0.0f, 0.0f}; // each axis clamped to -1 through 1
    glm::vec2 aimDirection = {0.0f, 0.0f};
    bool jumpPressed = false;
    bool jumpHeld = false;
    bool primaryAttackPressed = false;
};
```

Platformer movement reads only `direction.x`; flying movement reads both axes.
`aimDirection` is independent from movement and does not need to be normalized by the
controller. Facing remains left or right for sprite flipping and follows the horizontal
aim component independently from movement. `primaryAttackPressed` uses the actor's
configured primary attack:
the player and a ranged NPC fire a projectile, while a biting NPC begins a bite. This
keeps controllers and brains independent of concrete attack types.

GLFW events update held, pressed, and released button state. Pressed and released
edges remain pending until the first fixed update consumes them, even if a rendered
frame performs no fixed update. Held input is available to every fixed update.

The application converts the mouse from window points to framebuffer pixels, removes
the integer-scaled game's letterbox margin, and then converts internal screen position
through the camera into a player aim direction. Clicks outside the game viewport are
ignored. NPC behaviour writes a direction toward the target into the same intention,
so the ranged attack system does not distinguish player and NPC controllers.

When ImGui captures the keyboard, gameplay intentions are empty except for the key
that closes the inventory.

## Time and update order

Simulation runs at a fixed 60 Hz (`1.0f / 60.0f`). Rendering runs at the available
frame rate. Frame time is clamped to 0.25 seconds before entering the accumulator to
avoid an excessive catch-up after a breakpoint or stall.

The fixed update order is:

1. Observe the world and update NPC target memory.
2. Update NPC FSMs, destinations, and paths.
3. Produce player and NPC intentions.
4. Move every actor and resolve tile collision.
5. Advance every attack and evaluate active bite hitboxes.
6. Move projectiles and find their earliest collision.
7. Detect pickups and level-exit overlap.
8. Apply queued damage, collection, and level-completion requests.
9. Begin deaths caused by damage applied this tick.
10. Apply queued spawn, removal, and respawn requests.

World collections do not change while a system is traversing them. Systems append
plain requests to `WorldRequests`; `World` applies them at the end of the tick.

`updateWorldSimulation` owns the fixed gameplay-system order and creates a request queue
for each tick. A game supplies its `TileMap`, `World`, player intentions, and elapsed time
without reproducing the engine update sequence. Game-specific animation clips and
animation updating remain in the game layer as a separate presentation step.

## Platformer movement

Movement has one readable configuration and one small runtime state:

```cpp
struct PlatformerMovementConfig
{
    float maximumSpeed;
    float groundAcceleration;
    float airAcceleration;
    float groundDeceleration;
    float jumpSpeed;
    float gravity;
    float jumpReleaseGravity;
    float maximumFallSpeed;
    float coyoteTime;
    float jumpBufferTime;
};

struct PlatformerMovement
{
    PlatformerMovementConfig config;
    bool grounded = false;
    float coyoteRemaining = 0.0f;
    float jumpBufferRemaining = 0.0f;
};
```

`PlatformerMovementSystem` is one module with short functions for timers, horizontal
acceleration, starting a jump, and gravity. It produces velocity, collision moves the
body, and movement observes returned contacts. There is no general ability framework.

## Flying movement

```cpp
struct FlyingMovement
{
    float speed = 60.0f;
};
```

`FlyingMovementSystem` normalizes a nonzero two-dimensional intention direction,
multiplies it by speed, and moves the body through the same tile collision system. It
has no gravity, jumping, acceleration, or separate physics rules. This is sufficient
for the flying NPC to follow ordinary four-direction grid paths while still respecting
walls.

## Tile map and collision

The first TileMap is one rectangular row-major vector of integer tile IDs. Tile ID
zero is empty. Each nonzero definition provides a sprite region and whether it is
solid. The same layer supplies rendering and collision.

Early tests and examples construct maps from ASCII strings, using `.` for empty and
`#` for solid. JSON loading arrives later. Entities, pickups, player spawn, and exit
are separate level data rather than special tile IDs.

Collision moves an arbitrary-sized actor AABB along X, resolves against nearby full
solid tile AABBs, then repeats along Y. It returns left, right, ground, and ceiling
contacts. Actors do not physically collide with or push other actors.

Left, right, and bottom map boundaries behave as solid. The top is open. A configurable
kill height remains as a safety check for invalid or exceptional positions.

## Camera

`Camera` is the final lightweight view used by rendering and coordinate conversion.
`CameraController` retains the previous view position. It begins locked to the centre
of the player's collider, then moves only enough to return the player's centre to a
configurable dead zone. The example uses an 80 by 45 internal-pixel dead zone.

The view remains clamped to the tile map bounds. Small maps are centred on the relevant
axis. Camera movement is rounded to internal pixels to keep pixel art stable. The game
updates the controller after world simulation so rendering and mouse aiming share the
same final camera snapshot. The F1 debug overlay draws the viewport bounds in cyan and
the dead zone in yellow.

## NPC sensing and target memory

An NPC detects the living player only when the player is within its configurable
notice distance and a tile segment cast reports clear line of sight. On detection it
stores the player's `ActorId` and last seen feet. After losing sight it continues
chasing the last seen feet for a configurable memory duration, initially 1.5 seconds,
then returns to patrol.

```cpp
struct NpcSenses
{
    float noticeDistance = 96.0f;
    float forgetAfter = 1.5f;
};
```

Line of sight reuses the tile segment-cast foundation used by projectile collision,
but ignores actors. Sensing, memory expiry, reacquisition, and the inability to target
a dead player are pure tested behaviour.

## NPC patrol and finite state machine

The example includes three NPCs:

- A ground creature that patrols, uses platformer path search, jumps, chases, and bites.
- A flying creature that patrols, uses ordinary four-direction grid paths, chases, and
  bites.
- A ranged creature that patrols, chases the player, then stops and fires projectiles
  while the player remains visible.

All three use the same concrete enum-and-switch FSM. A patrol has two authored feet-based
endpoints. The NPC pathfinds to the current endpoint and swaps endpoints after arriving.
The example NPC factories also take a feet-based spawn point and patrol, so a level can
place any number of zombies, bats, and zombie soldiers. Their shared factory accepts an
explicit collision-body size: ground NPCs currently use `12 x 20` pixels while bats use
the smaller `12 x 8` body. Sprite dimensions remain independent of these gameplay bodies.
Ground sprites use a feet anchor; the bat uses a centre anchor so its small collider sits
in the middle of its larger sprite without changing feet-based navigation coordinates.

```cpp
enum class NpcState
{
    Idle,
    Patrol,
    Chase,
    Bite
};

struct NpcBrain
{
    NpcState state = NpcState::Idle;
    float stateTime = 0.0f;
    std::optional<ActorId> target;
    glm::vec2 lastSeenTargetFeet;
    float targetMemoryRemaining = 0.0f;
};

struct Patrol
{
    glm::vec2 firstFeet;
    glm::vec2 secondFeet;
    bool headingToSecond = true;
};
```

The state transitions are explicit:

- Idle enters Patrol when a patrol is configured.
- Patrol follows its current endpoint and enters Chase when the player is detected.
- Chase follows the player's last seen feet, enters Bite when a visible player is in
  bite range and the NPC has a `BiteAttack`, or stops and requests an attack when a
  visible player can be shot with a `RangedWeapon`. It returns to Patrol when target
  memory expires. An NPC without an attack continues chasing at close range.
- Bite faces the player, stops horizontal input, requests the bite once, and returns
  to Chase after windup, active, and recovery phases complete.

The bite does not lunge. Bite range must include the configured forward hitbox rather
than using an unrelated centre-to-centre distance.

The FSM chooses a goal or primary action; it does not directly manipulate velocity.

- The FSM chooses a state and destination.
- Lowest-cost search chooses navigation steps.
- `PathFollower` converts the next step to `InputIntentions`.
- `ActorSystem` sends those intentions to the actor's platformer or flying movement
  system.

Paths are recalculated when the target enters another grid cell, subject to a 0.25
second cooldown. Patrol paths also recalculate when the active endpoint changes or a
path becomes invalid. If a patrol endpoint is unreachable, the NPC stops and retries
after the cooldown; it never walks directly toward an endpoint without a valid path.
Flying followers reduce their input magnitude for the final fraction of a movement
step, reaching each waypoint before turning around a solid-tile corner.

### Future improvement: movement-specific navigation anchors

The current navigation code uses actor feet for both ground and flying NPCs. Feet are
the natural reference point for a platformer actor because they identify the surface
the actor is standing on, but they are an artificial reference point for a flying
actor whose body moves freely through a grid cell.

A future cleanup should make the distinction explicit:

- Platformer navigation converts `feetOf(body.bounds)` to a grid cell and follows
  waypoints positioned at the feet of standable cells.
- Flying navigation converts `centerOf(body.bounds)` to a grid cell and follows the
  centre of each empty cell.
- `Patrol::firstFeet` and `Patrol::secondFeet` become the neutral `firstPoint` and
  `secondPoint`. For a ground NPC these points describe feet; for a flying NPC they
  describe body centres.

This keeps the useful feet convention for platformer movement without forcing it onto
flyers. It does not change `Aabb::position`, which remains the body's top-left corner,
and it does not couple navigation to the sprite anchor. The flying corner regression
tests should remain in place while making this change.

## Navigation

The generic weighted path search works on `GridPosition` and receives a neighbour
function. It has no knowledge of tiles, actors, or platformer physics. With no
heuristic it performs Dijkstra's lowest-cost search. Supplying an admissible heuristic
uses A* ordering instead:

```cpp
findLowestCostPath(start, goal, neighbors);
findLowestCostPath(start, goal, neighbors, manhattanHeuristic);
```

Students can add or remove the final argument to compare the two searches. A heuristic
must use the same cost unit as its connections and must never overestimate the remaining
cost. Manhattan distance is valid for the flying policy's four-direction unit-cost
connections. Platformer navigation instead uses `platformerTickHeuristic`, an
optimistic estimate in simulation ticks. It considers only the minimum horizontal
distance to enter the goal column at maximum speed. It ignores acceleration, braking,
obstacles, and vertical travel, so it cannot overestimate the remaining cost. Remove
the heuristic argument inside `findPlatformerPath` to compare it with Dijkstra's search.

- A flying policy supplies four-direction neighbours through empty cells.
- A platformer policy treats standable foot cells as positions and supplies Walk,
  Fall, and Jump neighbours.

Each neighbour records its destination, traversal type, cost, and any inputs needed
to perform the connection. Path search preserves that information in its result rather
than returning only a list of cells:

```cpp
enum class Traversal
{
    Fly,
    Walk,
    Fall,
    Jump
};

struct InputStep
{
    float duration;
    InputIntentions intentions;
};

struct NavigationStep
{
    GridPosition destination;
    Traversal traversal;
    InputProgram inputs;
};
```

`InputProgram` durations are seconds. The fixed-step loop remains seconds-based; no
integer tick counter is added to movement. At runtime the path follower uses elapsed
seconds to replay each program one fixed update at a time. Before replaying a jump or
fall, it uses normal movement intentions to approach the takeoff cell and brake. The
program begins only when the actor is grounded, within the one-pixel arrival tolerance,
and horizontally stopped. Path following never changes an actor's position or velocity
directly.

Platformer jumping is an isolated advanced subsystem. For each candidate direction
and short or fully held jump, neighbour generation copies the actor's body size and
`PlatformerMovementConfig`, then runs the real `updatePlatformerMovement` function at
the fixed 60 Hz step. A connection is accepted only when collision reports a landing
on another standable cell. Fall connections are generated by the same simulation and
walk connections join continuously walkable standable cells on the same row. Offering
longer Walk connections lets the search compare one continuous walk, with braking only
at its destination, against a Jump or Fall connection. A generated airborne program also
includes the short grounded deceleration after landing, preventing a later route from
starting while the actor still carries velocity from the previous connection.
Walk connections are simulated through the same movement and path-following functions,
including their final braking. Walk, Fall, and Jump costs therefore all count fixed
simulation updates.

These neighbours are generated on demand rather than stored as an editor-visible
graph. Tests replay generated programs through both the path follower and the real
movement system, including an end-to-end ground NPC jump through
`updateWorldSimulation`. This guards against navigation, path following, system order,
and runtime movement drifting apart.

## NPC bite

NPC bite is an optional primary-attack component with explicit phases:

```cpp
enum class BitePhase
{
    Ready,
    Windup,
    Active,
    Recovery
};

struct BiteAttack
{
    int damage = 1;
    glm::vec2 hitboxSize = {10.0f, 8.0f};
    float reach = 4.0f;
    float windupDuration = 0.12f;
    float activeDuration = 0.08f;
    float recoveryDuration = 0.30f;

    BitePhase phase = BitePhase::Ready;
    float phaseTimeRemaining = 0.0f;
    std::vector<ActorId> actorsHit;
};
```

The windup telegraphs the attack, the active phase places the configured hitbox AABB
in front of the NPC according to facing, and recovery prevents immediate repetition.
The NPC does not lunge. An eligible actor can take damage at most once in one bite,
and bite damage adds no knockback. Idle overlap with an NPC is harmless; this is
intentionally different from Platformer's permanent whole-body contact damage.

Once windup begins, the bite completes even if the target moves away or becomes
occluded. The forward active hitbox can miss. Cancelling a committed attack based on
fresh perception would couple sensing to attack timing and make the telegraph unreliable.

Bite hitboxes are evaluated after every actor has moved, so actor iteration order
cannot decide whether a bite reaches its target. Hits append damage requests rather
than changing health immediately.

There is no general post-hit invulnerability in the first version. A bite records each
actor it has hit and can therefore damage that actor only once before returning to
Ready. A projectile disappears on its first collision, and bite recovery limits attack
frequency.

## Projectiles and damage

Ranged attacks use a small explicit `Ready -> Shoot -> Recovery -> Ready` sequence.
Entering Shoot creates exactly one projectile and holds the Attack animation for the
configured shoot duration. Recovery prevents another projectile until the weapon
returns to Ready. This makes the firing pose readable without coupling projectile
creation to animation frames.

The first projectile contains bounds, velocity, damage, remaining lifetime, owner,
team, and a sprite. Its collision bounds and sprite display size are independently
configurable world-pixel dimensions. The ranged attack system normalizes the actor's
aim intention, spawns the projectile beyond the actor body along that direction, and
supports the full 360-degree range. Each tick it casts the swept
segment from its previous to proposed position against solid tiles and eligible actor
AABBs, selects the earliest hit, applies one damage request, and disappears. A ranged
NPC uses the same component and attack system as the player; its brain only supplies
the attack intention. Projectiles do not pierce, bounce, home, or cause splash damage.

## Life and death

When health reaches zero, an actor changes from Alive to Dying for a configurable
short duration, initially 0.4 seconds. A dying actor cannot decide, accept gameplay
input, fire, or take another hit. Gravity and tile collision continue. Animation
selection gives Death highest priority.

At the end of the timer an NPC is removed. A player is replaced or reset at the
level's feet-based spawn with restored health and runtime movement state.

## Animation

Sprites use named clips such as Idle, Move, Jump, Fall, Attack, and Death. There is no
animation state machine. Bite and ranged-weapon systems retain their distinct gameplay
behaviour, but both select the actor's Attack clip. A pure priority function selects a
clip from actor life state, an active attack, grounded state, and velocity. Death has
highest priority, followed by Attack. A ranged weapon selects Attack throughout its
Shoot phase, then returns to the appropriate movement animation during Recovery.
Render-scene tests cover selection, source frame, sprite placement, and
horizontal flipping.

Each animated actor owns a small optional `Animator` component containing its current
animation, elapsed playback time, and an `AnimationSet`. The set maps animation names
to that character's own clips, so characters do not have to share atlas rows, frame
counts, or layouts. The engine's `updateActorAnimations` presentation system reads
actor life, combat, and movement state to select and advance these clips. It remains a
separate call after `updateWorldSimulation` because animation does not affect gameplay.
The game layer defines separate sets for the soldier player,
zombie, bat, and zombie soldier using explicit source regions; the engine neither
assigns rows nor requires matching layouts. These sets live in
`app/example_animations.*`, where their real atlas data can also be exercised by
focused tests. The example game is supplied with one finished `160 x 216` atlas.
Each actor uses two rows of four `32 x 24` frames, so wide attacks and death poses
do not need to be reduced to fit a long single row. The fifth column holds a
dedicated passing pose for the player, zombie, and zombie soldier without replacing
their idle frames. Ground walk cycles play contact A, passing, contact B, passing.
The bat's movement cycle plays wings up, out, down, and recovery at 0.10 seconds
per frame. Its two additional poses occupy the fifth column of its two atlas rows.
The source coordinates match
integer coordinates in image editors such as Piskel. Artwork sources and atlas tooling
are deliberately kept outside the student repository. The high-resolution originals
are downsampled once while building the atlas; runtime frame dimensions then match
their world-pixel display dimensions without stretching.
Game and behaviour code select named animations; the reusable animation helper finds
the clip in the actor's set, advances playback, and writes the selected frame to the
actor's `Sprite`.

### Future improvement: mixed-size animation frames

`SpriteRegion` already describes an arbitrary source rectangle, and `AnimationClip`
does not require every region to have the same dimensions. The example deliberately
uses fixed `32 x 24` regions because animation playback currently changes only
`Sprite::region`; `Sprite::size` remains fixed. Mixed-size regions would therefore be
stretched into the same display rectangle.

A future extension can replace each clip's bare `SpriteRegion` with an
`AnimationFrame` containing a source region, display size, and pivot or offset. This
would preserve pixel-for-pixel mixed-size artwork while keeping feet stable between
frames. The actor's collision body must remain independent from these visual frame
dimensions.

## Inventory, pickups, and exit

Inventory has a configurable number of slots. Each slot is empty or contains an item
stack. Item definitions configure name, icon, maximum stack, effect kind, and effect
amount. New behaviour remains an explicit C++ `switch`; data does not become a hidden
scripting system.

Adding an item first fills compatible stacks and then empty slots. Anything that does
not fit remains in the world. The result reports added and remaining counts.

The example includes coins, health potions, and a key. The ImGui HUD shows health and
important counts. Pressing I pauses simulation and opens an inventory with selection
and a Use button. UI emits requests rather than changing the world directly.

An exit has a configurable optional item requirement, count, and consume flag. The
example level requires one key and does not consume it. Reaching an unlocked exit
completes the level.

## Rendering

Gameplay objects never issue graphics calls. A pure render-scene builder reads the
world and camera and produces ordered `SpriteDrawCommand` values. Tests cover camera
transforms, visible tile selection, animation frames, sprite placement, facing flips,
and draw order.

Texture dimensions and `SpriteRegion` rectangles are measured in source-image pixels.
`Sprite::size` and `Body::bounds` are measured in world pixels and are independent;
matching sizes must be assigned explicitly. Rendering aligns differently sized body
and sprite rectangles at the actor's feet.

A small OpenGL `SpriteRenderer` submits textured quads using one uncomplicated shader.
There is no scene graph, material system, lighting, or general render graph. ImGui is
drawn after the internally scaled game image so HUD and inventory remain crisp at the
window resolution. The example application's F1 key toggles app-only actor debugging:
each player or NPC gets a separate ImGui window, while white sprite bounds and red
collision bounds are drawn over the game. The reusable debug-data builder stays
separate from the GLFW/ImGui presentation code so it can be tested without a window.

## Loading and errors

The first phases construct maps and definitions in C++. Data loading is added only
after runtime behaviour works. Loaders parse JSON into plain data structures, validate
dimensions, IDs, ranges, references, and required fields, then construct runtime
objects.

Malformed required assets throw descriptive exceptions. Startup catches once, prints
the error, and exits. There are no silent defaults or fallback textures for required
content.

## Testing strategy

Catch2 tests are grouped by subsystem. Major coverage includes:

- coordinate and feet conversions;
- fixed-step accumulation and input-edge consumption;
- actor ID uniqueness, lookup, and removal;
- movement acceleration, deceleration, air control, jumping, buffers, coyote time,
  jump cutting, gravity, and fall-speed limits;
- flying movement normalization, speed, two-axis intentions, and tile collision;
- collision on all four sides, corners, arbitrary body sizes, map bounds, and high
  allowed speeds;
- camera dead-zone following, clamping, centring, and pixel rounding;
- NPC distance and line-of-sight sensing, target memory, patrol endpoint changes, FSM
  transitions, bite eligibility, and path-recalculation cooldown;
- generic lowest-cost search, optional Manhattan heuristic, flying neighbours,
  standability, falls, jump clearance, and runtime replay of accepted jump arcs;
- ranged shoot and recovery timing, one projectile per shot, and firing direction;
- bite windup, active and recovery timing, forward hitbox placement, one hit per
  attack, committed attacks that can miss, harmless idle overlap, no lunge, no general
  post-hit invulnerability, and deferred damage;
- projectile lifetime, owner/team filtering, earliest swept hit, damage, and removal;
- health, dying behaviour, NPC removal, and player respawn;
- inventory stacking, capacity, partial collection, item use, and exit requirements;
- animation selection and render-scene command generation;
- valid and invalid data loading once JSON is introduced.

Rendering submission and platform UI integration receive a documented manual smoke
test. Tests use public behaviour wherever practical; small pure helpers can be tested
directly when they represent a concept students are expected to learn.

## Implementation phases

Each phase must configure, build, and pass all tests before the next begins.

1. **Foundation**: CMake targets, vendored dependencies, math conventions, fixed-step
   loop, and test harness.
2. **Tile world**: ASCII TileMap, AABB, X-then-Y collision, arbitrary body sizes, map
   boundaries, and tests.
3. **Player movement**: input state, two-dimensional `InputIntentions`, consolidated
   platformer movement, facing, and movement tests.
4. **Rendering and camera**: textures, named animation clips, render-scene commands,
   simple OpenGL sprites, fixed internal resolution, and dead-zone camera.
5. **Composition and lifecycle**: Actor aggregate, stable IDs, world requests, health,
   death, removal, and player respawn.
6. **Combat**: player ranged weapon, left/right projectiles, NPC bite phases and
   hitboxes, segment casts, damage, teams, and combat tests.
7. **NPC behaviour and path search**: sensing, line of sight, target memory, two-point
   patrols, concrete NPC FSM, generic lowest-cost search, flying movement and navigation,
   path follower, unreachable-path retry, and intention-driven NPC movement and attacks.
8. **Platformer navigation**: standable cells, walking, falling, jump arcs, clearance,
   runtime replay tests, and jumping ground NPC.
9. **Inventory and level loop**: pickups, stacks, configurable effects, key requirement,
   exit, ImGui HUD, and paused inventory.
10. **Data-driven content**: vendored `nlohmann/json`, validated loaders for maps,
    actors, items, animation clips, and example-level data.
11. **Teaching polish**: complete example level, concise architecture-linked README,
    diagrams, manual graphics smoke test, and a documented route for converting phases
    into student exercises.

## Definition of done

- macOS Apple Clang and Windows Visual Studio build the C++17 project from the vendored
  repository using documented CMake commands.
- The complete example can run, jump, scroll, shoot an NPC, collect and use an item,
  collect a key, open the inventory, die and respawn, and reach the exit.
- A flying NPC demonstrates ordinary grid path search, patrols, detects and remembers the
  player, chases, and performs a timed bite.
- A ground NPC follows a path that includes at least one successful jump and exercises
  the same patrol, chase, and bite FSM.
- A ranged NPC stops and fires straight projectiles while it can see the player.
- All automated tests pass without a graphics context.
- The documented manual graphics smoke test passes.
- The code and documentation preserve the boundaries in this document.
