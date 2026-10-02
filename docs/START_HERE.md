# Start Here

This is the recommended first route through Simple Platformer. It follows one player
update from the application into the engine, then follows the resulting world back to
the renderer. You do not need to understand every subsystem before changing the game.

Build and run the project using the instructions in [README.md](../README.md) before
starting the tour. Keep a matching test file open beside each implementation file;
the tests often provide the smallest examples of how a subject is meant to be used.
Each step below names a starting test and a question to answer before moving on.

Two short references help while reading. The [glossary](GLOSSARY.md) gives the meaning
of the words the code uses, and [C++ style](CPP_STYLE.md) explains how the code is
written and the language features worth knowing first.

There are two routes through this document:

- If you are new to the engine, follow the
  [recommended reading route](#recommended-reading-route) first.
- If you are starting a project requirement, use the
  [platformer gameplay](#platformer-gameplay-requirements) or
  [enemy behaviour](#enemy-behaviour-requirements) starting points.

## Everyday development loop

Keep each change small enough to verify directly:

1. Change the subject that owns the rule.
2. Build using the platform instructions in [README.md](../README.md).
3. Run its focused test while working, then run the complete test suite. See
   [Running focused tests](../README.md#running-focused-tests) for commands.
4. Launch the example game when the change affects interaction or presentation.
5. When a change might cost time, such as more NPCs or a larger level, launch the release
   build and check gameplay responsiveness. See the
   [release build instructions](../README.md#macos-configure-build-and-test).

The focused tests provide fast feedback about one rule. The complete suite checks its
interaction with the rest of the engine, while running the game covers presentation
and graphics behaviour that automated tests deliberately leave out.

## The big picture

The outer flow is:

```text
main.cpp
  -> runApplication()
  -> Game::update()
  -> updateWorldSimulation()
  -> gameplay systems
```

Rendering follows a separate path:

```text
Game::buildScene()
  -> RenderScene (plain draw data)
  -> SpriteRenderer (OpenGL)
```

This separation lets most game behaviour run in tests without opening a window.

## Starting project work

Platformer mechanics and enemy behaviour share the same movement, collision, combat, and
input systems: an NPC produces the same `InputIntentions` that the application produces
for the player. Each requirement below names the steps of the
[reading route](#recommended-reading-route) it needs, what to change without code, and
the [ARCHITECTURE.md](ARCHITECTURE.md#extension-recipes-for-project-work) recipe to
follow when you extend the engine.

### Platformer gameplay requirements

```text
keyboard and mouse
  -> InputState
  -> InputIntentions
  -> PlatformerMovement
  -> collision
  -> Body
```

- **Read** steps 3–5: the core data model, player movement, and physics. Read the
  tests for the variable-height jump, coyote time, and jump buffer before changing
  those rules.
- **Tune** speed, acceleration, braking, gravity, and jump settings in
  [`actors.json`](../assets/catalogs/actors.json), and feel the difference in the example
  game.
- **Add** one focused ability, such as a double jump, dash, wall slide, or wall jump,
  with the [movement-ability recipe](ARCHITECTURE.md#adding-a-movement-ability).
- **Build a game** from the mechanics by composing actors, authoring levels
  ([CONTENT.md](CONTENT.md)), adding pickups or combat rules, and giving animation and
  HUD feedback.

Movement work does not require reading NPC or navigation code.

### Enemy behaviour requirements

```text
senses and memory
  -> finite-state decision
  -> pathfinding and path following
  -> InputIntentions
  -> the same movement and combat systems
```

- **Read** steps 7 and 8: NPC behaviour and combat, then navigation. Enemies move through
  the movement and physics paths in steps 4 and 5, so return to those when one does not
  move as intended.
- **Tune** `noticeDistance`, `standoffDistance`, `targetMemoryDuration`, and
  `searchDuration` in an actor's `senses` in [`actors.json`](../assets/catalogs/actors.json).
  The [debug overlay](../README.md#debug-overlay) shows visible targets, remembered
  positions, patrol points, goals, and paths.
- **Add** an enemy with the [enemy recipe](ARCHITECTURE.md#creating-a-new-enemy), which
  starts from the smallest route that works, or a reusable built-in state with the
  [NPC-state recipe](ARCHITECTURE.md#adding-an-npc-state).

## Recommended reading route

### 1. Find the outside of the program

Start with [`app/main.cpp`](../app/main.cpp), then skim
[`app/application.cpp`](../app/application.cpp). The application owns the window, input,
fixed-step loop, UI, and graphics setup. Start at `runApplication` and find the callback
that calls `game.update`. Do not worry about the OpenGL details yet.

**Starting test:** "Elapsed time is simulated in fixed 60 Hz updates" in
[`test_fixed_step.cpp`](../tests/timing/test_fixed_step.cpp). How many simulation updates
can happen during one rendered frame?

### 2. See what the game coordinates

Read [`app/game/game.hpp`](../app/game/game.hpp) and
[`app/game/game.cpp`](../app/game/game.cpp). `Game` is the seam between the application
and the engine: it passes input into the simulation, updates presentation state, changes
levels, and builds a scene for rendering. Start with `Game::update` and `Game::buildScene`;
return to construction and level-loading helpers in step 9. What it and `GameLevel` own
is listed under [Application folders](ARCHITECTURE.md#application-folders).

Then open [`assets/levels/levels.json`](../assets/levels/levels.json), which selects the
starting level and maps IDs to files. Follow its first entry into
[`level_1.json`](../assets/levels/level_1.json). For now, notice the terrain, player spawn,
and actor placements: level data becomes a tile map and a world of actors. Return to
parsing and composition in step 9, after learning the data they build.

**Starting test:** "World simulation advances its shared clock once per update" in
[`test_world_simulation.cpp`](../tests/world/test_world_simulation.cpp). Which call in
`Game` advances gameplay, and which builds the draw data?

### 3. Learn the core data model

Read these headers first:

- [`actor.hpp`](../include/simple_platformer/actor/actor.hpp) shows an actor assembled
  from optional components;
- [`world.hpp`](../include/simple_platformer/world/world.hpp) shows what the world owns;
- [`body.hpp`](../include/simple_platformer/physics/body.hpp) shows the physical state;
- [`input_state.hpp`](../include/simple_platformer/input/input_state.hpp) shows the common
  intentions used by the player and NPCs.

The important idea is composition: gameplay roles are not represented by different
actor subclasses. Each actor is assembled from a different combination of data. The
full recipe is in [Actor composition](ARCHITECTURE.md#actor-composition).

**Starting test:** "The actor builder gives exactly one movement component" in
[`test_actor_builder.cpp`](../tests/support/test_actor_builder.cpp). Which data does a
platformer have that a flyer does not?

### 4. Follow one simulation tick

Read [`world_simulation.cpp`](../src/world/world_simulation.cpp). It is the short,
authoritative list of gameplay systems and their order. From there, follow only the
system relevant to the feature you are studying.

For the player movement path, a useful order is:

1. [`input_state.cpp`](../src/input/input_state.cpp)
2. [`actor_system.cpp`](../src/actor/actor_system.cpp), which dispatches actor movement
3. [`platformer_movement.cpp`](../src/movement/platformer_movement.cpp)

Read [`test_platformer_movement.cpp`](../tests/movement/test_platformer_movement.cpp)
beside the movement implementation for examples of acceleration and jumping. Start with
"Ground movement accelerates and decelerates", then "Grounded actors can jump".
Which intention starts a jump, and which body field changes?

For input, read "A pressed edge is consumed by only one fixed update" in
[`test_input_state.cpp`](../tests/input/test_input_state.cpp). Why does holding a button
differ from pressing it this tick?

### 5. Understand physics and tile collision

Follow the movement loop into physics:

1. [`body.hpp`](../include/simple_platformer/physics/body.hpp) defines bounds and velocity;
2. [`body.cpp`](../src/physics/body.cpp): read `applyGravity`, `moveBody`, then `sweepAxis`;
3. [`tile_map.cpp`](../src/world/tile_map.cpp) defines which tiles block movement.

`moveBody` moves horizontally, then vertically. Each sweep stops at the first blocking
tile and clears velocity on that axis. Read
[`test_body.cpp`](../tests/physics/test_body.cpp) beside the implementation. Start with
"Horizontal movement stops on either side of a solid tile". Where does the body stop,
and what happens to its velocity? Then read "Collision resolves X before Y at a corner".
See [Tile map, collision, and validation](ARCHITECTURE.md#tile-map-collision-and-validation)
for more detail.

### 6. Follow presentation separately

Read [`render_scene.hpp`](../include/simple_platformer/render/render_scene.hpp) first:
`SpriteDrawCommand` names each piece of draw data, such as position, rotation, and
opacity. Then read these after the movement loop:

1. [`render_scene.cpp`](../src/render/render_scene.cpp) converts world state into plain
   sprite draw commands, including pickup bobbing and timed feedback such as hit flashes;
2. [`camera.cpp`](../src/render/camera.cpp) follows the player and converts world space to
   screen space;
3. [`presentation.cpp`](../src/render/presentation.cpp) runs the presentation systems
   after the simulation, such as
   [`animation_system.cpp`](../src/render/animation_system.cpp), which selects and
   advances actor animation clips;
4. [`sprite_renderer.cpp`](../app/graphics/sprite_renderer.cpp) submits the finished draw
   commands to OpenGL.

The first three can be understood and tested without knowing OpenGL.

**Starting test:** "A render scene contains visible tiles followed by the player" in
[`test_render_scene.cpp`](../tests/render/test_render_scene.cpp). Which fields place the
player on screen, and where does draw order come from?

### 7. Read NPC behaviour and combat

NPCs use the same actor movement and attack systems as the player. Their brain produces
intentions instead of reading a keyboard. Start with
[`npc.hpp`](../include/simple_platformer/npc/npc.hpp) for the brain, perception, and state
data, then [`npc_system.cpp`](../src/npc/npc_system.cpp) for the decision order.

Follow just one case: a `Pursuer` on patrol sees the player outside attack range and
starts chasing.

```text
Patrol -> sees player -> Chase -> movement intentions
```

1. [`npc_senses.cpp`](../src/npc/npc_senses.cpp): `observeTarget` records visibility and
   remembers the player's position.
2. [`npc_facts.cpp`](../src/npc/npc_facts.cpp): `gatherNpcFacts` records that the target is
   known and visible.
3. [`npc_transitions.cpp`](../src/npc/npc_transitions.cpp): in `nextNpcState`, the
   `Patrol` case uses `pursuit` to choose `Chase` when no attack can reach the target.
4. [`npc_states.cpp`](../src/npc/npc_states.cpp): `enterNpcState` changes the state, then
   `updateChaseState` asks for intentions to reach the remembered position.
5. Return to [`actor_system.cpp`](../src/actor/actor_system.cpp): it sends those intentions
   through the movement code from step 4. Step 8 explains how a path becomes intentions.

**Starting tests:** "NPC sight observes distance and solid tiles" in
[`test_npc_senses.cpp`](../tests/npc/test_npc_senses.cpp), then "A known target is chased
from idle and from patrol" in
[`test_npc_transitions.cpp`](../tests/npc/test_npc_transitions.cpp). What observation
becomes a fact, and how does that fact change Patrol to Chase?

Then skim [`attack_system.cpp`](../src/combat/attack_system.cpp) and
[`projectile_system.cpp`](../src/combat/projectile_system.cpp) for how attack intentions
produce hits. [`world_requests.cpp`](../src/world/world_requests.cpp) applies their damage and
handles death together with queued removals and spawns. Start with "Damage is deferred
until world requests are applied" in
[`test_world_requests.cpp`](../tests/world/test_world_requests.cpp). When does a hit
actually reduce health? Other NPC tactics and states can wait until this case makes sense.

### 8. Follow basic navigation

First understand NPC decisions and ordinary movement. Keep this introduction to A* and
platformer walking, falling, and jumping:

1. [`route.hpp`](../include/simple_platformer/navigation/route.hpp) names the search's
   locations and connections. For this reading, each location is a cell's floor.
   [`navigation_path.hpp`](../include/simple_platformer/navigation/navigation_path.hpp)
   shows the waypoints an actor follows in world coordinates.
2. [`route_search.cpp`](../src/navigation/route_search.cpp): start at
   `findLowestCostRoute`. A* takes the location with the cheapest estimated total cost,
   checks whether it reached the goal, then considers its outgoing connections. The
   estimate combines the cost so far and a heuristic that never overestimates the
   remaining cost. Follow the main loop, then `relax`, then `reconstructRoute`.
3. [`platformer_cells.cpp`](../src/navigation/platformer_cells.cpp): follow `canStandAt`
   for where the body fits with floor support.
4. [`actor_navigation.cpp`](../src/navigation/actor_navigation.cpp): follow
   `findActorPath` into `findPlatformerPath`. It searches the available connections and
   turns a successful route into waypoints; an unreachable goal has no path.
5. [`path_follower.cpp`](../src/navigation/path_follower.cpp): start at
   `followPlatformerPath`, then `followWalkStep` and `followAirborneStep`. Walking
   approaches and brakes at a waypoint. Falling and jumping first reach their takeoff
   point, then replay recorded intentions and wait for landing.
6. [`platformer_connections.cpp`](../src/navigation/platformer_connections.cpp): follow
   `buildPlatformerConnections`, `planPlatformerConnections`, `simulateWalk`, and
   `simulateAirborneTraversal` to see where those connections and recorded inputs come
   from. Leave climbing branches for a later reading.

Connections are found by running the real movement and collision code. The follower
produces intentions; the movement system still moves the body. [Navigation](ARCHITECTURE.md#navigation)
is the detailed reference when you need more than this basic route.

**Starting tests:** "A search chooses by the connections' costs, not by how many steps a
route takes" in [`test_route_search.cpp`](../tests/navigation/test_route_search.cpp).
Why can a longer route be cheaper? Then read "A platformer path follower approaches and
brakes without moving the body directly" and "A platformer path follower executes a
generated jump through movement and collision" in
[`test_path_follower.cpp`](../tests/navigation/test_path_follower.cpp). Which part chooses
intentions, and which part changes the body's position?

### 9. Complete the level loop

Finally, read the small inventory and world-object subjects:

- [`inventory.cpp`](../src/inventory/inventory.cpp)
- [`item_use.cpp`](../src/inventory/item_use.cpp)
- [`pickup.cpp`](../src/world/pickup.cpp)
- [`level_exit.cpp`](../src/world/level_exit.cpp)
- [`world_requests.cpp`](../src/world/world_requests.cpp)

These show automatic pickup, deferred world changes, item use, and level completion.

**Starting test:** "An entered exit completes only once it has had time to open" in
[`test_level_exit.cpp`](../tests/world/test_level_exit.cpp). What starts opening the door,
and what completes the level?

Now return to [`level_composition.cpp`](../app/game/level_composition.cpp):
`composeGameLevel` builds the map and world from level data and catalog definitions.
Read [`level_data.cpp`](../app/content/level_data.cpp) afterward, starting with
`loadLevelData` and `parseLevelData`, to see how JSON becomes that data. Use
[CONTENT.md](CONTENT.md) for the JSON fields and shared catalogs.

Start with "A level composes an actor from its catalog definition" in
[`test_level_composition.cpp`](../tests/app/game/test_level_composition.cpp). Which data
comes from the catalog, and which comes from the level placement?

## What to skip on a first reading

It is safe to return later to:

- climbing and flying navigation;
- OpenGL setup and shader details in `app/graphics`;
- ImGui layout code in `app/ui` and `app/debug`;
- cover fading, which fades NPCs and pickups standing in grass on the player's screen
  ([Tile map, collision, and validation](ARCHITECTURE.md#tile-map-collision-and-validation));
- the connection table, which builds platformer connections when a level starts and
  rebuilds them where a tile breaks ([The connection table](ARCHITECTURE.md#the-connection-table));
- atlas coordinates and clip timings in `assets/catalogs/animations.json`;
- CI, formatting, and static-analysis targets.

Once the route above makes sense, use [ARCHITECTURE.md](ARCHITECTURE.md) as the
detailed reference for boundaries, ownership, conventions, and design trade-offs.
