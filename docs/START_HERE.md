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

- Read [`app/main.cpp`](../app/main.cpp).
- Skim [`app/application.cpp`](../app/application.cpp), starting at `runApplication`.
  Follow `preparePlayerInput`, then `runGameUpdates` to the callback that calls
  `game.update`. Clearing buttons and resetting accumulated time are separate steps.
- Notice what the application owns: the window, input, fixed-step loop, UI, and graphics
  setup.
- **Starting test:** "Elapsed time is simulated in fixed 60 Hz updates" in
  [`test_fixed_step.cpp`](../tests/timing/test_fixed_step.cpp).
- **Check:** How many simulation updates can happen during one rendered frame?

### 2. See what the game coordinates

- Read [`app/game/game.hpp`](../app/game/game.hpp).
- In [`app/game/game.cpp`](../app/game/game.cpp), read `Game::update` and
  `Game::buildScene`. Notice how `Game` passes input into simulation, updates presentation,
  changes levels, and builds draw data.
- Open [`levels.json`](../assets/levels/levels.json): it selects the starting level and
  maps level IDs to files.
- Open [`level_1.json`](../assets/levels/level_1.json): identify the terrain, player spawn,
  and actor placements.
- **Starting test:** "World simulation advances its shared clock once per update" in
  [`test_world_simulation.cpp`](../tests/world/test_world_simulation.cpp).
- **Check:** Which call in `Game` advances gameplay, and which builds the draw data?

### 3. Learn the core data model

- Read [`actor.hpp`](../include/simple_platformer/actor/actor.hpp): an actor is assembled
  from components. Its role comes from the data it has.
- Read [`world.hpp`](../include/simple_platformer/world/world.hpp): the world owns the
  actors and other objects in a level.
- Read [`body.hpp`](../include/simple_platformer/physics/body.hpp): bounds and velocity
  describe physical state.
- Read [`input_state.hpp`](../include/simple_platformer/input/input_state.hpp): the player
  and NPCs produce the same `InputIntentions`.
- **Starting test:** "The actor builder gives exactly one movement component" in
  [`test_actor_builder.cpp`](../tests/support/test_actor_builder.cpp).
- **Check:** Which data does a platformer have that a flyer does not?

### 4. Follow one simulation tick

- Read [`world_simulation.cpp`](../src/world/world_simulation.cpp) for the systems and
  their execution order.
- Read [`input_state.cpp`](../src/input/input_state.cpp): held buttons and pending presses
  become intentions for one tick.
- Read [`actor_system.cpp`](../src/actor/actor_system.cpp): each actor's movement component
  selects the movement function.
- Read [`platformer_movement.cpp`](../src/movement/platformer_movement.cpp), starting at
  `updatePlatformerMovement`. Follow acceleration, jump timing, and gravity into body
  movement.
- **Starting test:** "A pressed edge is consumed by only one fixed update" in
  [`test_input_state.cpp`](../tests/input/test_input_state.cpp).
- **Movement tests:** "Ground movement accelerates and decelerates", then "Grounded actors
  can jump" in
  [`test_platformer_movement.cpp`](../tests/movement/test_platformer_movement.cpp).
- **Check:** How does holding a button differ from pressing it this tick? Which intention
  starts a jump, and which body field changes?

### 5. Understand physics and tile collision

- Read [`body.hpp`](../include/simple_platformer/physics/body.hpp) for bounds, velocity,
  and collision contacts.
- In [`body.cpp`](../src/physics/body.cpp), read `applyGravity`, `moveBody`, then
  `sweepAxis`.
- Trace the horizontal move, then the vertical move. Each sweep finds how far the body
  can move before a blocking tile; `moveBody` applies that distance and clears velocity
  on the blocked axis.
- In [`tile_map.cpp`](../src/world/tile_map.cpp), read `blocksMovement` for tile and map
  boundary rules.
- **Starting tests:** "Horizontal movement stops on either side of a solid tile", then
  "Collision resolves X before Y at a corner" in
  [`test_body.cpp`](../tests/physics/test_body.cpp).
- See the [worked collision example](ARCHITECTURE.md#worked-example-moving-right-into-a-wall)
  for a sweep into a wall.
- **Check:** Where does the body stop, and what happens to its velocity?

### 6. Follow presentation separately

- Read [`render_scene.hpp`](../include/simple_platformer/render/render_scene.hpp):
  `SpriteDrawCommand` names draw data such as position, rotation, and opacity.
- Read [`render_scene.cpp`](../src/render/render_scene.cpp), starting at `buildRenderScene`.
  Follow how world objects become sprite draw commands.
- Read [`camera.cpp`](../src/render/camera.cpp): the camera follows the player and converts
  world positions to screen positions.
- Read [`presentation.cpp`](../src/render/presentation.cpp), then
  [`animation_system.cpp`](../src/render/animation_system.cpp): presentation selects and
  advances animation after simulation.
- Skim [`sprite_renderer.cpp`](../app/graphics/sprite_renderer.cpp) for where finished draw
  commands are submitted to OpenGL.
- **Starting test:** "A render scene contains visible tiles followed by the player" in
  [`test_render_scene.cpp`](../tests/render/test_render_scene.cpp).
- **Check:** Which fields place the player on screen, and where does draw order come from?

### 7. Read NPC behaviour and combat

Trace one case: a `Pursuer` on patrol, with no ranged weapon, sees the player
outside bite range and chases.

```text
Patrol -> sees player -> Chase -> movement intentions
```

- Read [`npc.hpp`](../include/simple_platformer/npc/npc.hpp) for brain, perception, and
  state data.
- Read [`npc_system.cpp`](../src/npc/npc_system.cpp) for the decision order.
- In [`npc_senses.cpp`](../src/npc/npc_senses.cpp), read `observeTarget`: it records
  `targetVisible = true` and remembers the player's position and ID.
- In [`npc_facts.cpp`](../src/npc/npc_facts.cpp), read `gatherNpcFacts`: the target is
  known and visible, but neither in bite range nor in sights for a ranged attack.
- In [`pursuer.cpp`](../src/npc/pursuer.cpp), follow the `Patrol` case in
  `nextPursuerState`. `pursuit` chooses `Chase` when no attack can reach the target.
- In [`npc_behaviour.cpp`](../src/npc/npc_behaviour.cpp), `enterNpcState` resets the state
  timer and clears the old path. Back in `pursuer.cpp`, follow `updatePursuerState`
  into `updateChaseState`: it requests movement intentions to reach the remembered position.
- In [`actor_system.cpp`](../src/actor/actor_system.cpp), follow those intentions into
  the same movement functions used by the player.
- **Starting tests:** "NPC sight observes distance and solid tiles" in
  [`test_npc_senses.cpp`](../tests/npc/test_npc_senses.cpp), then "A known target is chased
  from idle and from patrol" in
  [`test_pursuer.cpp`](../tests/npc/test_pursuer.cpp).
- **Check:** What observation becomes a fact, and how does that fact change Patrol to Chase?
- Skim [`attack_system.cpp`](../src/combat/attack_system.cpp) and
  [`projectile_system.cpp`](../src/combat/projectile_system.cpp): attack intentions produce
  hits and queue damage. See the [attack timing example](ARCHITECTURE.md#attack-timing-example)
  for phase advancement and hit checks across a large update.
- Read [`actor_lifecycle.cpp`](../src/actor/actor_lifecycle.cpp): `applyDamageRequests`
  reduces health and starts fatal deaths. `updateActorLifecycle` also advances existing
  death timers, respawns the player, and queues NPC removal.
- Read [`world_requests.cpp`](../src/world/world_requests.cpp) for applying queued
  removals and spawns. It also applies damage without advancing death timers, so requests
  can be applied while paused.
- **Combat tests:** "Damage is deferred until world requests are applied" in
  [`test_world_requests.cpp`](../tests/world/test_world_requests.cpp), then "Fatal damage
  begins a timed death" in
  [`test_actor_lifecycle.cpp`](../tests/actor/test_actor_lifecycle.cpp).
- **Check:** When does a hit reduce health, and when does a dying actor's timer advance?

### 8. Follow basic navigation

- Read [`route.hpp`](../include/simple_platformer/navigation/route.hpp) for search
  locations and connections. Focus on locations on a cell's floor.
- Read [`navigation_path.hpp`](../include/simple_platformer/navigation/navigation_path.hpp)
  for the waypoints an actor follows in world coordinates.
- In [`route_search.cpp`](../src/navigation/route_search.cpp), start at
  `findLowestCostRoute`. Follow the main loop, then `relax`, then `reconstructRoute`.
- Trace A*: take the location with the cheapest estimated total cost, check for the goal,
  then consider outgoing connections. The estimate combines cost so far with a heuristic
  that never overestimates remaining cost.
- **Search test:** "A search chooses by the connections' costs, not by how many steps a
  route takes" in [`test_route_search.cpp`](../tests/navigation/test_route_search.cpp).
- **Check:** Why can a longer route be cheaper?
- In [`platformer_cells.cpp`](../src/navigation/platformer_cells.cpp), read `canStandAt`
  for where the body fits with floor support.
- In [`actor_navigation.cpp`](../src/navigation/actor_navigation.cpp), follow
  `findActorPath` into `findPlatformerPath`: search connections and turn a successful
  route into waypoints. An unreachable goal has no path.
- In [`path_follower.cpp`](../src/navigation/path_follower.cpp), read
  `followPlatformerPath`, then `followWalkStep` and `followAirborneStep`.
- Trace walking: approach and brake at a waypoint. Trace falling and jumping: reach the
  takeoff point, replay recorded intentions, then wait for landing.
- In [`platformer_connections.cpp`](../src/navigation/platformer_connections.cpp), read
  `buildPlatformerConnections`, `planPlatformerConnections`, `simulateWalk`, and
  `simulateAirborneTraversal`. These run real movement and collision to build connections
  and record inputs for falling and jumping.
- **Following tests:** "A platformer path follower approaches and brakes without moving
  the body directly", then "A platformer path follower executes a generated jump through
  movement and collision" in
  [`test_path_follower.cpp`](../tests/navigation/test_path_follower.cpp).
- **Check:** Which part chooses intentions, and which part changes the body's position?

### 9. Complete the level loop

- Read [`inventory.cpp`](../src/inventory/inventory.cpp) for adding and removing item
  stacks.
- Read [`item_use.cpp`](../src/inventory/item_use.cpp) for applying an item's effect.
- Read [`pickup.cpp`](../src/world/pickup.cpp) for automatic collection.
- Read [`level_exit.cpp`](../src/world/level_exit.cpp) for unlocking, opening, and
  completing an exit.
- Read [`world_requests.cpp`](../src/world/world_requests.cpp) for applying collection,
  item use, and queued world changes.
- **Exit test:** "An entered exit completes only once it has had time to open" in
  [`test_level_exit.cpp`](../tests/world/test_level_exit.cpp).
- **Check:** What starts opening the door, and what completes the level?
- In [`level_composition.cpp`](../app/game/level_composition.cpp), read `composeGameLevel`:
  level data and catalog definitions become a map and world.
- In [`level_data.cpp`](../app/content/level_data.cpp), start with `loadLevelData` and
  `parseLevelData`. Follow `jsonLevelData`: `jsonLegends` parses typed templates,
  `jsonExplicitPlacements` reads positioned objects, then `placeMapObjects` copies
  templates at marked cells. The placement pass works with C++ values; JSON is read once.
- **Composition test:** "A level composes an actor from its catalog definition" in
  [`test_level_composition.cpp`](../tests/app/game/test_level_composition.cpp).
- **Check:** Which data comes from the catalog, and which comes from the level placement?

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
