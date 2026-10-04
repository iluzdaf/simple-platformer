# Architecture

The main boundaries, ownership rules, and update order in Simple Platformer.
Start with [START_HERE.md](START_HERE.md) for a route through the code. Use
[CONTENT.md](CONTENT.md) for JSON fields and [CPP_STYLE.md](CPP_STYLE.md) for C++ conventions.

## Project shape

- C++17, with gameplay rules that can run without a window.
- Structs hold state; namespace functions update it. Classes own collections and resources.
- Third-party source lives in `external/`; versions and licenses are in
  [THIRD_PARTY.md](../THIRD_PARTY.md).

| Target                    | What it owns                                   | Dependencies                                |
| ------------------------- | ---------------------------------------------- | ------------------------------------------- |
| `simple_platformer_core`  | Simulation and plain render-scene data         | GLM; no window, graphics API, or JSON       |
| `simple_platformer`       | Application, content loading, graphics, and UI | Core, GLFW, glad, ImGui, stb, nlohmann/json |
| `simple_platformer_tests` | Core and application tests that need no window | Core, Catch2, nlohmann/json                 |

### Application folders

| Location              | Responsibility                                                                          |
| --------------------- | --------------------------------------------------------------------------------------- |
| `app/application.cpp` | Window events, input, fixed steps, UI requests, and rendering                           |
| `app/game`            | Session flow and level composition; `Game` owns the current level, catalogs, and camera |
| `app/content`         | Plain content definitions, JSON loaders, catalogs, and validators                       |
| `app/graphics`        | Window and graphics resources, viewport conversion, and sprite submission               |
| `app/ui`              | HUD, inventory, and completion UI                                                       |
| `app/debug`           | Debug snapshots and their display                                                       |
| `assets`              | Level geometry, placements, and shared definitions                                      |

`GameLevel` keeps the level number, map, world, player spawn, and actor definition names
together. Replacing it starts a fresh world.

## Runtime flow

- `application.cpp` gathers player input and applies UI requests before simulation.
- `FixedStep` runs at 60 Hz and clamps incoming frame time to 0.25 seconds.
- `Game::update` writes player intentions, runs simulation, handles completion, then
  updates the camera and presentation.
- Rendering reads the resulting state at the available frame rate.
- `preparePlayerInput` clears buttons and pending edges while inventory is open,
  the game is complete, play is interrupted, or the UI captures the keyboard.
  Without a gameplay cursor, it clears only attack input.
- `runGameUpdates` resets accumulated time while inventory is open, the game is
  complete, or play is interrupted. UI capture alone does not stop simulation.
- Drawing continues while inventory is open; animations and cover fades wait for
  simulation to resume.

[`updateWorldSimulation`](../src/world/world_simulation.cpp) owns this order:

| Order | Work                                                                     |
| ----- | ------------------------------------------------------------------------ |
| 1     | Return if complete; otherwise advance the world clock                    |
| 2     | If the exit is opening, update only the exit and return                  |
| 3     | Rebuild navigation connections affected by recorded tile breaks          |
| 4     | Update NPC senses, memory, decisions, and intentions                     |
| 5     | Move actors, then pickups, resolving tile collision                      |
| 6     | Update attacks, projectiles, and existing projectile bursts              |
| 7     | Apply damage, advance death timers, detect pickups, apply queued changes |
| 8     | Check the exit                                                           |

- Systems update existing objects while iterating. Spawns and removals go into
  `WorldRequests` and are applied after iteration.
- `updateActorLifecycle` alone applies damage and owns death timers and respawning.
  `WorldRequests` holds deferred changes; `applyWorldRequests` applies item use,
  collection, spawns, and removals.
- Simulation calls `updateActorLifecycle`, then `updatePickups`, then
  `applyWorldRequests`. This lets pickups see damage and respawns before queued changes
  alter the lists. For inventory clicks, `drawInterface` returns a slot request; the
  application passes it to `Game::useInventoryItem`, which calls `applyWorldRequests`
  without advancing timers or detecting new pickups.
- Tile breaks change the map during projectile updates; navigation reads them before
  the next NPC decision or search.

## Coordinates and time

| Value           | Meaning                                                                             |
| --------------- | ----------------------------------------------------------------------------------- |
| World axes      | X points right; Y points down                                                       |
| `Aabb::topLeft` | The corner physics moves and measures collision from                                |
| Feet            | Middle of a body's bottom edge; used for spawns, placements, patrols, and waypoints |
| Cell            | A tile position, calculated using the map's configured `tileSize`                   |
| Internal image  | 320 × 180 pixels, integer-scaled and letterboxed in the window                      |
| `deltaTime`     | Seconds for one update, passed as `float`                                           |
| Component timer | A countdown or elapsed duration advanced by its owning system                       |
| World stamp     | An optional `double` recording when an event happened on the world clock            |

- Name the point being moved: `topLeft`, `feetOf`, `boxStandingOn`, or `moveFeetTo`.
- Use a timer when one system owns a window's start and end. Use a stamp when several
  readers ask how long ago an event happened.
- `World::secondsSince` returns a stamp's age, or nothing if unset. Future stamps are invalid.
- The clock advances once per active simulation update. Drawing never advances it.
- Timers and stamps belong to their current world; level transitions start fresh runtime state.

## World ownership and identity

- `World` owns actors, projectiles, bursts, pickups, item definitions, the exit,
  noise events, the simulation clock, and the platformer connection table.
- `TileMap` belongs to `GameLevel`, alongside `World`.
- `World::addActor` assigns an `ActorId`. Zero is invalid; IDs are not reused within a world.
- `findActor` uses a linear search. Keep IDs across updates and look up actors when needed.
- Pointers and references are temporary views. Adding or removing objects, restarting,
  or replacing a level can invalidate them.

## Actor composition

Every [`Actor`](../include/simple_platformer/actor/actor.hpp) has a body, intentions,
facing, team, and life state. Players and NPCs use the same type.

| Capability           | Components                                               | Rule                                             |
| -------------------- | -------------------------------------------------------- | ------------------------------------------------ |
| Movement             | `PlatformerMovement` or `FlyingMovement`                 | Exactly one                                      |
| Climbing             | `SurfaceClimb`                                           | Requires platformer movement                     |
| NPC control          | `NpcBrain`, `NpcPerception`, `NpcSenses`, `PathFollower` | Required together; `Patrol` is optional          |
| Primary attack       | `BiteAttack` or `RangedWeapon`                           | At most one                                      |
| Contact damage       | `ContactDamage`                                          | Independent of the primary attack                |
| Animation            | `Sprite` and `Animator`                                  | An animator requires a sprite and animation data |
| Health and inventory | `Health`, `Inventory`                                    | Optional                                         |

- Attack components require a non-neutral team.
- `World::addActor` validates component combinations. Level validation checks placement.
- Systems read the components they need. No actor subclass or virtual dispatch is required.

## Input and movement

- Player input and NPC states produce the same `InputIntentions`.
- `InputState` keeps button edges until a fixed update consumes them.
- `InputProgram` stores timed intentions for navigation to replay.
- Mouse aiming passes through the display viewport and camera into world coordinates.
- Facing follows horizontal aim, then intended movement, then keeps its previous value.

| Movement         | Rule                                                                                                                     |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------ |
| Platformer       | Horizontal acceleration and braking, variable-height jumps, coyote time, jump buffering, gravity, and a fall-speed limit |
| Flying           | Normalised two-axis input at a configured speed; no gravity                                                              |
| Surface climbing | Hold climbable walls or ceilings; losing contact or releasing the grip returns to platformer movement                    |

- All movement uses `moveBody` for tile collision. Pickups use it too.
- `avoidLedges` stops grounded walking before unsupported floor; deliberate jumps still work.
- `climbGrip` is `Hold`, `Release`, or `Keep`. `Keep` leaves the current grip alone.
- Movement records blocked travel for NPC decisions. `contactDamage` separately asks
  combat to enable overlap damage.

## Tile map, collision, and validation

- `TileMap` is a rectangular, row-major array of tile IDs. Zero is empty.
- Tile definitions choose movement blocking, sight blocking, climbing, and what a
  breakable tile becomes.
- Actors, pickups, and exits are separate objects. The loader parses each object-legend
  template once into typed settings. It reads explicit placements separately, then copies
  a template at each marked cell and supplies its position. Marker terrain is empty.
- Template references keep their authored legend paths, including unused entries, so
  composition can check every catalog reference. Map placement handles player and exit
  uniqueness with the original cell paths.

| Example tile | Movement and projectiles | Sight |
| ------------ | ------------------------ | ----- |
| Empty        | Pass                     | Pass  |
| Stone        | Block                    | Block |
| Glass        | Block                    | Pass  |
| Grass        | Pass                     | Block |

- Tile collision is handled by `moveBody` in [`body.cpp`](../src/physics/body.cpp).
  `sweepHorizontalCollision` checks X, then `sweepVerticalCollision` checks Y from the
  new position. Each sweep finds the first surface an arbitrary-sized AABB would meet;
  `moveBody` stops travel there, zeroes velocity on that axis, and reports `CollisionContacts`.
- `touchingSurfaces` and `touchingClimbableSurfaces` reuse those sweeps as short contact
  probes. The climbable filter finds grip surfaces; movement collision uses blocking tiles.
- Left, right, and bottom map boundaries block movement; the top is open.
- Actors do not push or physically collide with one another.
- Segment casts find the first tile or AABB along a line. Projectiles use movement
  blocking; sensing uses sight blocking.
- A projectile breaks a tile only if its weapon enables breaking and the tile names
  a replacement. The map records each break.
- Spawn and respawn positions need body clearance; platformers also need ground support.
  Patrol points need clearance and, for platformers without climbing, support.

### Worked example: moving right into a wall

With 16-pixel tiles, a 12-pixel-wide body starts at `x = 20` and requests a
20-pixel move right. In its row, column 2 is empty and column 3 is solid.

```text
x (pixels)      20          32  36  40      48  52          64
Before          [===========]               |################
Requested                           [=======|XXX]############
Stopped                         [===========|################
```

- `|` marks the wall at 48; `X` shows the overlap the requested move would cause.
- `sweepHorizontalCollision` starts at the body's right edge (`rightEdge = 32`), skips empty
  column 2, and finds the wall in column 3.
- The allowed distance is `distance = 3 * 16 - 32 = 16` pixels.
- `moveBody` moves the body to `x = 36`, sets `contacts.right`, clears horizontal
  velocity, then sweeps vertically.

### Advanced reading: segment casts

The public contracts in [`segment_cast.hpp`](../include/simple_platformer/physics/segment_cast.hpp)
are enough to use casts for projectiles and sensing. Study the intersection math and
cover spans when changing collision geometry or the rules for seeing out of cover.

In [`segment_cast.cpp`](../src/physics/segment_cast.cpp), the two tile casts have
separate reading paths:

- `segmentCastMovementBlockingTiles` scans candidate cells, expands each blocking tile
  for the moving body's size, and keeps the earliest intersection.
- `segmentCastSightBlockingTiles` reads `sightBlockingSpans`, then
  `firstHitAfterStartingCover`. Spans use fractions along the line, from 0 to 1.
  Sorting puts them in travel order. Touching or overlapping spans from 0 form the
  starting cover; the first span after a gap blocks sight. An exact shared corner
  keeps that cover connected.

`segmentTileRange` and `tileBox` supply candidate cells and tile bounds. `segmentSpan`
and `castAxis` contain the shared intersection math. The movement cast uses the entry
fraction; sight also needs the leave fraction to find where starting cover ends.

## NPC behaviour

| Part                | What it holds or does                                                    |
| ------------------- | ------------------------------------------------------------------------ |
| `NpcSenses`         | Notice distance, standoff distance, memory duration, and search duration |
| `NpcPerception`     | Visibility and heard-landing results, replaced each sensing update       |
| `NpcBrain`          | State, tactic, timing, and remembered target ID and feet                 |
| `NpcFacts`          | A snapshot gathered from perception, memory, movement, and attacks       |
| `npc_behaviour.cpp` | Dispatches transitions and actions; resets time and path on state entry  |
| Tactic files        | Transitions, entry actions, and state updates for one tactic             |

- Sight detects the living opponent player within notice distance and with clear line of sight.
- Shots and landings emit noise with the source's feet at emission. The next sensing
  update shares the batch with every NPC, then discards it.
- Shots can be heard through walls. Landings require a grounded observer on the same
  supported ground run. Both use notice distance.
- Fresh sight takes priority over heard positions. Without either, target memory counts down.
- Entering a state resets its time and path. Movement and combat execute its intentions later.
- Transition functions return a state to enter, or `std::nullopt` to keep the current
  state. Staying keeps the path, runs the state's actions, and advances its elapsed time.
- All tactics share `NpcState`. A tactic's switch handles states supplied by callers,
  including states that tactic does not normally choose. See `nextPursuerState` for an example.

| Tactic                                       | Choice                                                                              |
| -------------------------------------------- | ----------------------------------------------------------------------------------- |
| [Pursuer](../src/npc/pursuer.cpp)            | Chase, attack when able, then search where the target was lost                      |
| [KeepDistance](../src/npc/keep_distance.cpp) | Back away inside standoff distance; watch from its position after losing the target |
| [Coward](../src/npc/coward.cpp)              | Flee from a nearby visible threat and bite when it reaches the forward hitbox       |
| [Charger](../src/npc/charger.cpp)            | Wake on a landing, charge in a fixed direction, then recover when blocked           |

Tactics choose built-in states. New behaviour needs the corresponding facts, state,
and components before a tactic can select it.

Each tactic has a matching test file, such as `test_pursuer.cpp`, covering its
transitions and behaviour. Sensing, facts, and system validation have separate tests.
`test_npc_behaviour.cpp` covers the shared state-entry resets.

## Navigation

- `findActorPath` takes an actor, goal feet, map, step, and connection table.
- A flyer starts in its feet cell. A platformer starts where its bounds rest on a floor,
  wall, or ceiling; an airborne platformer has no starting location.
- A route location is a cell and surface. A cell's floor, walls, and ceiling are separate places.
- `route_search` uses A* to find the cheapest location in the goal cell. Connections
  have positive costs; the heuristic must never exceed the real remaining cost.
  A zero heuristic gives Dijkstra's search.
- Reaching the goal cell returns `Found`, waypoints, and the remaining distance to the
  requested point. Exhausting reachable locations returns `Unreachable` with no path.
  An invalid starting location returns no result.

| Movement   | Connections                                                                   | Cost                                                   |
| ---------- | ----------------------------------------------------------------------------- | ------------------------------------------------------ |
| Flying     | Four neighbouring open cells                                                  | One per cell; Manhattan heuristic                      |
| Platformer | Walks, falls, jumps, and optional climbs found by real movement and collision | Simulated ticks; each search adds a jump-start penalty |

### The connection table

- `PlatformerTraversalProfile` contains body size, movement configuration, simulation
  step, and optional climbing configuration.
- `PlatformerConnectionTable` stores every cell's connections per profile. Actors with
  equal profiles share them.
- `prepareNavigation` builds NPC profiles when the level starts. A new profile builds
  on its first search.
- Each cell keeps the footprint its connection simulations read or swept. A tile break
  rebuilds only cells whose footprints include it.
- NPCs plan again after a break or when their goal moves far enough from the planned goal.

### Following a path

| Traversal    | What the follower asks for                                                  |
| ------------ | --------------------------------------------------------------------------- |
| Fly          | Steer toward each waypoint without overshooting                             |
| Walk         | Walk and brake into the waypoint                                            |
| Fall or Jump | Stop at takeoff, then replay the recorded input program                     |
| Climb        | Reach the starting surface, hold it, then replay the recorded input program |

- The follower emits intentions; movement changes the body. It never teleports it or
  writes velocity.
- A failed traversal clears the path so the NPC can plan again.
- Replay tests run generated programs through ordinary movement to check that planning
  and execution agree.

## Combat, projectiles, and life cycle

| Subject        | Rule                                                                                                    |
| -------------- | ------------------------------------------------------------------------------------------------------- |
| Bite           | Ready → Windup → Active → Recovery; a forward hitbox damages each opponent once per bite                |
| Ranged weapon  | Ready → Shoot → Recovery; entering Shoot queues one projectile in the aim direction                     |
| Contact damage | While requested, body overlap damages each opponent once; releasing clears the hit history              |
| Projectile     | Swept collision chooses the earliest blocking tile or eligible actor; a hit or expired lifetime ends it |
| Burst          | A short visual effect queued when a projectile ends; no damage or collision                             |
| Damage         | Queued by combat, applied by lifecycle, and stamped on the world clock                                  |
| Death          | Health reaching zero enters Dying; an explicit timer ends in NPC removal or player respawn              |

- Owner and team filtering prevent projectiles hitting the shooter or allies.
- Dying actors receive no gameplay intentions or further damage; movement and collision continue.
- Respawn restores player health and movement state at the stored spawn feet. Inventory persists.
- Death timing is independent of animation length.

### Attack timing example

A default bite follows these phases in
[`attack_system.cpp`](../src/combat/attack_system.cpp):

```text
Ready -> Windup (0.12 s) -> Active (0.08 s) -> Recovery (0.30 s) -> Ready
                           hit check
```

- Starting a bite gives Windup its full timer; it advances on later updates.
- `advanceBite` counts down the current timer, then enters the next phase with its full
  timer. Each update advances at most one phase and discards leftover time.
- `applyBiteHits` queues damage only during Active, once per opponent. Windup delays
  the hit; Recovery delays the next attack.
- The durations total 0.50 s. Transitions happen on update boundaries, so the actual
  bite can take slightly longer.
- NPC decisions run before attacks. A newly entered Bite waits for combat to process
  its request before Ready can mean the bite finished.

See [`test_bite_attack.cpp`](../tests/combat/test_bite_attack.cpp) for phase advancement
and hit checks.

## Inventory, pickups, and levels

- Inventory fills compatible stacks, then empty slots, and reports what did not fit.
- Item effects use an explicit C++ switch.
- Pickups fall onto tiles. The living player collects them on body overlap; excess
  quantity remains when inventory is full.
- Exits can require and consume an item. Opening is latched so it cannot consume twice.
- While an exit opens, only the world clock and exit check advance. The door and player
  effects read that clock.
- `Game` replaces the level after completion, carrying only player health and inventory.
  The camera resets. The final exit completes the game; restart loads a fresh starting level.

### Data-driven level boundary

| Step                 | Owner                            | Result                                                                |
| -------------------- | -------------------------------- | --------------------------------------------------------------------- |
| Load shared catalogs | `app/content/game_catalogs.cpp`  | Definitions checked against the atlas size and reused for the session |
| Load a level         | `app/content/level_data.cpp`     | Plain `LevelData`, with objects placed at marked cells                |
| Compose the level    | `app/game/level_composition.cpp` | Names resolved into a map, world, and placed objects                  |
| Start the level      | `Game::startLevel`               | Player inserted, placements validated, camera and navigation prepared |

JSON stays in `app/content`. Core systems receive C++ values; runtime IDs, velocities,
paths, and timers start fresh. [CONTENT.md](CONTENT.md) describes the stored fields.

## Presentation

- `Game::buildScene` returns ordered `SpriteDrawCommand` values. `SpriteRenderer`
  submits them to OpenGL.
- `updateWorldPresentation` advances actor animation and cover fades after simulation.
  It pauses while the exit opens.
- Scene construction reads state for pickup bobbing, hit flashes, death fading, and bursts.
- `CameraController` follows a dead zone, clamps to the map, and rounds to internal pixels.
- `DisplayViewport` is shared by rendering, aiming, HUD, and debug UI.
- `drawInterface` returns requests; the application applies them. Item use can be applied
  while inventory is open without advancing simulation.
- `Game::debugOverlay` collects plain diagnostics; ImGui projects the snapshot into the viewport.

| Size           | Meaning                                                            |
| -------------- | ------------------------------------------------------------------ |
| Texture        | Full atlas dimensions in source pixels                             |
| `SpriteRegion` | A source rectangle; drawn at one world pixel per source pixel      |
| Body bounds    | Collision dimensions, independent of the sprite                    |
| Sprite anchor  | Places art at the body's feet or centre; does not change collision |

### Animation

- JSON defines clips. C++ selects them from life, attack, grounded, and velocity state,
  with death before attack.
- Each actor owns its animator and clip data. Frames in one set share dimensions.
- A climber holding a surface counts as grounded. Its art turns onto the surface;
  its collision body stays axis-aligned.

### Cover

- NPC sight uses line of sight. Screen visibility separately fades NPCs and pickups
  according to body coverage by sight-blocking tiles, unless the player can see them.
- The player's sprite darkens in cover rather than disappearing. NPC sight or a recent
  shot exposes it.
- Cover fading reads sensing results and shot stamps; hearing uses noise events.

## Extension recipes for project work

### Adding a movement ability

- Add needed input to `InputState` and `InputIntentions`.
- Keep configuration and runtime state separate. Define how it interacts with jumping,
  gravity, and ordinary controls.
- Use the existing collision path. `SurfaceClimb` shows how an optional component can
  fall back to platformer movement.
- Select animation from the resulting state. Test the ability's start, end, and reset.

### Adding an NPC state

- Add the enum value and facts its decisions need.
- Add decisions and actions in the tactic's file, such as `pursuer.cpp`.
- Request movement and attacks through intentions; add its name to debug presentation.
- Test transitions with facts, then behaviour through the NPC update.

### Creating a new enemy

- Start with an actor definition and placement using existing components and tactics.
- Add C++ capabilities, facts, or states only when the definition cannot express it.

## Error handling and validation

| Boundary                           | What it checks                                                                 |
| ---------------------------------- | ------------------------------------------------------------------------------ |
| JSON loaders                       | Types, required and unknown fields, ranges, and source paths                   |
| Content validators and composition | Authoring rules and cross-file references, including unused catalog entries    |
| Core validators                    | Runtime values and component combinations, regardless of how they were created |
| Level validation                   | Body clearance and support against the composed map                            |

- `content_json` reads shapes; `content_diagnostics` builds field paths and errors without
  depending on JSON. Typed validators can run without parsing.
- Missing optional fields keep defaults; present invalid values fail.
- Invalid content or programmer input throws at its boundary. Required assets fail
  startup with a message; they do not fall back to placeholders.

## Testing

- Tests follow source subjects. Start with the smallest test at the layer owning the rule.
- Use local data or `tests/support` helpers; parser tests use strings and `tests/fixtures`.
- Supplied-content checks validate the campaign without fixing its enemy counts or item values.
- Run the game for OpenGL and ImGui integration. Build, test, and quality commands are
  in [README.md](../README.md); the everyday loop is in [START_HERE.md](START_HERE.md#everyday-development-loop).
