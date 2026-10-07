# Start Here

This is the recommended first route through Simple Platformer. It follows one player
update from the application into the engine, then follows the resulting world back to
the renderer. You do not need to understand every subsystem before changing the game.

Build and run the project using the instructions in [README.md](../README.md) before
starting the tour. Keep a matching test file open beside each implementation file;
the tests often provide the smallest examples of how a subject is meant to be used.
Each step below names a question to answer before moving on. Most also name a
starting test. Try each question before opening its **Answer** section.

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

- **Read** steps 6–8: NPC behaviour, combat, then navigation. Enemies move through
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
  Find the main loop: it reads input, updates the game through `runGameUpdates`,
  builds a scene with `game.buildScene`, and draws it.
- Notice what the application owns: the window, input, UI, and graphics setup.
- **Check:** Where does the application hand control to the game, and where does it
  draw the resulting scene?

<details>
<summary>Answer</summary>

`runGameUpdates` calls `Game::update` to advance the game. Back in `runApplication`,
`Game::buildScene` produces draw data and `SpriteRenderer::render` draws it.

</details>

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

<details>
<summary>Answer</summary>

`Game::update` calls `updateWorldSimulation` to advance gameplay.
`Game::buildScene` calls `buildRenderScene` to build draw data from the resulting state.

</details>

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

<details>
<summary>Answer</summary>

A platformer has `platformerMovement`, with grounded state, jump timers, and movement
configuration. A flyer has `flyingMovement` instead. `updateActorMovement` checks
these components to choose the movement function.

</details>

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

<details>
<summary>Answer</summary>

`InputState::consumeIntentions` keeps held input available each tick but consumes
pending presses once. `jumpPressed` requests a jump; `jumpHeld` controls its height.
When jumping is allowed, `startBufferedJump` sets `body.velocity.y` to negative
`jumpSpeed`, which moves upward. A buffered press can also start a later jump.

</details>

### 5. Understand physics and tile collision

- Read [`body.hpp`](../include/simple_platformer/physics/body.hpp) for bounds, velocity,
  and collision contacts.
- In [`body.cpp`](../src/physics/body.cpp), read `applyGravity`, `moveBody`, then
  `sweepHorizontalCollision` and `sweepVerticalCollision`.
- Trace the horizontal move, then the vertical move. Each sweep finds how far the body
  can move before a blocking tile; `moveBody` applies that distance and clears velocity
  on the blocked axis.
- In [`tile_map.cpp`](../src/world/tile_map.cpp), read `blocksMovement` for tile and map
  boundary rules.
- **Starting tests:** "Horizontal movement stops on either side of a solid tile", then
  "Collision resolves horizontal movement before vertical movement at a corner"
  in [`test_body.cpp`](../tests/physics/test_body.cpp).
- See the [worked collision example](ARCHITECTURE.md#worked-example-moving-right-into-a-wall)
  for a sweep into a wall.
- **Check:** Where does the body stop, and what happens to its velocity?

<details>
<summary>Answer</summary>

`sweepHorizontalCollision` and `sweepVerticalCollision` find the distance to the first blocking surface.
`moveBody` moves the body that far, leaving its leading edge at the surface, and zeroes velocity on the
blocked axis. It resolves X first, then Y from the new position.

</details>

### 6. Read NPC behaviour

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

<details>
<summary>Answer</summary>

`observeTarget` records visibility and remembers the target. `gatherNpcFacts`
produces `targetKnown` and `targetVisible`. In this example, neither attack-range
fact is true, so `pursuit` chooses `Chase`; `nextPursuerState` returns that choice
from `Patrol`. `enterNpcState` applies the transition.

</details>

### 7. Follow combat and actor life cycles

Follow an attack from its intention to damage, then death or respawn:

```text
attack intention -> hit check -> damage request -> health change
  -> fatal damage -> timed death -> NPC removal or player respawn
```

- Skim [`attack_system.cpp`](../src/combat/attack_system.cpp) and
  [`projectile_system.cpp`](../src/combat/projectile_system.cpp): attack intentions produce
  hits and queue damage. See the [attack timing example](ARCHITECTURE.md#attack-timing-example)
  for the bite phases and when they check for hits.
- Read [`actor_lifecycle.cpp`](../src/actor/actor_lifecycle.cpp): `updateActorLifecycle`
  applies queued damage, starts fatal deaths, advances existing death timers, respawns
  the player, and queues NPC removal.
- Read [`world_requests.cpp`](../src/world/world_requests.cpp) for applying queued
  removals, spawns, collection, and item use after lifecycle updates. UI item use can
  be applied while paused without advancing simulation.
- **Combat tests:** "Damage is deferred until actor lifecycle updates", then "Fatal damage
  begins a timed death" in
  [`test_actor_lifecycle.cpp`](../tests/actor/test_actor_lifecycle.cpp).
- **Check:** When does a hit reduce health, and when does a dying actor's timer advance?

<details>
<summary>Answer</summary>

A hit queues damage; `updateActorLifecycle` reduces health and advances timers only
for actors already dying when it began. A newly fatal hit therefore keeps its full
death timer. `applyWorldRequests` handles the queued removals and spawns afterward.

</details>

### 8. Follow basic navigation

Read navigation in three parts. The first two explain how an NPC chooses a route and
walks along it. You can then move on to step 9 and return to jumping and falling later.

#### 8.1 Choose a route

- Read [`route.hpp`](../include/simple_platformer/navigation/route.hpp) for search
  locations and connections. Focus on locations on a cell's floor.
- In [`route_search.cpp`](../src/navigation/route_search.cpp), start at
  `findLowestCostRoute`. Follow the main loop, then `relax`, then `reconstructRoute`.
- Trace A*: take the location with the cheapest estimated total cost, check for the goal,
  then consider outgoing connections. The estimate combines cost so far with a heuristic
  that never overestimates remaining cost.
- **Search test:** "A search chooses by the connections' costs, not by how many steps a
  route takes" in [`test_route_search.cpp`](../tests/navigation/test_route_search.cpp).
- **Check:** Why can a longer route be cheaper?

<details>
<summary>Answer</summary>

`findLowestCostRoute` compares the sum of connection costs, not the number of steps.
Its `relax` callback adds each connection's cost to the cost so far. Three steps
costing 1 each are cheaper than one step costing 5.

</details>

#### 8.2 Follow a walking route

- Read [`navigation_path.hpp`](../include/simple_platformer/navigation/navigation_path.hpp)
  for the waypoints an actor follows in world coordinates.
- In [`platformer_cells.cpp`](../src/navigation/platformer_cells.cpp), read `canStandAt`
  for where the body fits with floor support.
- In [`actor_navigation.cpp`](../src/navigation/actor_navigation.cpp), follow
  `findActorPath` into `findPlatformerPath`: search connections and turn a successful
  route into waypoints. An unreachable goal has no path.
- In [`path_follower.cpp`](../src/navigation/path_follower.cpp), read
  `followPlatformerPath`, then `followWalkStep` and `approachAndBrake`. Leave the other
  traversal types for later.
- Trace walking: approach and brake at a waypoint. The follower chooses intentions;
  the movement system changes the body's position.
- **Following test:** "A platformer path follower approaches and brakes without moving
  the body directly" in [`test_path_follower.cpp`](../tests/navigation/test_path_follower.cpp).
- **Check:** Which part chooses intentions, and which part changes the body's position?

<details>
<summary>Answer</summary>

`followPlatformerPath` calls `followWalkStep`, which uses `approachAndBrake` to
choose movement intentions. `updatePlatformerMovement` changes velocity and calls
`moveBody` to change position with collision checks.

</details>

#### 8.3 Generate and replay jumps and falls

- In [`platformer_connections.cpp`](../src/navigation/platformer_connections.cpp), read
  `buildPlatformerConnections`, `planPlatformerConnections`, `simulateWalk`, and
  `simulateAirborneTraversal`. These run real movement and collision to build connections
  and record inputs for falling and jumping.
- Return to [`path_follower.cpp`](../src/navigation/path_follower.cpp) and read
  `followAirborneStep`: reach the takeoff point, replay recorded intentions, then wait
  for landing.
- **Replay test:** "A platformer path follower executes a generated jump through
  movement and collision" in
  [`test_path_follower.cpp`](../tests/navigation/test_path_follower.cpp).
- **Check:** Where do the recorded intentions come from, and how does the NPC use them
  to repeat the planned jump?

<details>
<summary>Answer</summary>

`simulateAirborneTraversal` tries a jump or fall using real movement and collision.
`recordSimulationInput` saves the intentions used during that attempt. At runtime,
`followAirborneStep` approaches takeoff, uses `replayInput` to emit the saved
intentions, then waits for landing. Ordinary movement executes those intentions.

</details>

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

<details>
<summary>Answer</summary>

`updateLevelExit` starts opening when the living player overlaps an unlocked exit.
It consumes any required item when configured and records `openedTimeSeconds`.
On a later update, once `ExitOpenSeconds` has elapsed, it calls `World::completeLevel`.

</details>

## What to skip on a first reading

It is safe to return later to these topics, ordered from simpler details to more
advanced systems.

- [Atlas coordinates and clip timings](#atlas-coordinates-and-clip-timings)
- [ImGui layout](#imgui-layout)
- [Fixed-step timing](#fixed-step-timing)
- [Presentation](#presentation)
- [Level composition and JSON loading](#level-composition-and-json-loading)
- [CI, formatting, and static analysis](#ci-formatting-and-static-analysis)
- [Cover fading](#cover-fading)
- [Climbing and flying navigation](#climbing-and-flying-navigation)
- [The connection table](#the-connection-table)
- [OpenGL setup and shaders](#opengl-setup-and-shaders)

### Atlas coordinates and clip timings

- Open [`animations.json`](../assets/catalogs/animations.json) beside the
  [animation fields](CONTENT.md#animation-sets). Follow one clip's frame rectangles,
  frame duration, and looping setting.
- In [`animation_catalog.cpp`](../app/content/animation_catalog.cpp), read
  `parseAnimationCatalog`. In [`animation.cpp`](../src/render/animation.cpp), follow
  `updateAnimation` into `frameAt` to see how elapsed time selects a frame.
- **Starting test:** "Animation JSON preserves frame order timing and looping" in
  [`test_animation_catalog.cpp`](../tests/app/content/test_animation_catalog.cpp).
- **Check:** Does a frame's `position` place the actor in the world? What changes when
  you increase `frameDuration`?

<details>
<summary>Answer</summary>

A frame's `position` selects a rectangle within the atlas; the actor's body and sprite
anchor determine its world placement. `parseAnimationCatalog` reads the rectangles
and timing. `frameAt` uses elapsed time and `frameDuration` to select a frame, so a
larger duration plays the clip more slowly.

</details>

### ImGui layout

- Read [`inventory_layout.cpp`](../app/ui/inventory_layout.cpp), starting at
  `makeInventoryGridLayout`, for the inventory's rows and columns.
- In [`interface_ui.cpp`](../app/ui/interface_ui.cpp), follow `drawInventorySlot`
  and the grid layout into drawing and click handling. Then skim
  [`debug_overlay_ui.cpp`](../app/debug/debug_overlay_ui.cpp) for drawing debug labels
  and paths from a snapshot.
- **Starting test:** "Inventory grid dimensions follow its slot capacity" in
  [`test_inventory_layout.cpp`](../tests/app/ui/test_inventory_layout.cpp).
- **Manual check:** Open the inventory in the game, click a potion, and toggle F1 to
  inspect debug labels. The layout test does not exercise ImGui drawing or clicks.
- **Check:** How does an eight-slot inventory get its rows and columns? Does drawing
  a consumable slot apply its effect?

<details>
<summary>Answer</summary>

`makeInventoryGridLayout` uses at most three columns and rounds up the row count:
eight slots need three columns and three rows. `drawInventorySlot` reports a click;
the application passes the resulting request to `Game::useInventoryItem` to apply
the effect.

</details>

### Fixed-step timing

- In [`application.cpp`](../app/application.cpp), read `runGameUpdates` and follow
  its callback to `Game::update`.
- Read [`fixed_step.cpp`](../src/timing/fixed_step.cpp), starting at
  `FixedStep::advance`: frame time enters an accumulator, and each complete step
  triggers an update. Follow `FixedStep::reset` for discarding accumulated time.
- **Starting test:** "Elapsed time is simulated in fixed 60 Hz updates" in
  [`test_fixed_step.cpp`](../tests/timing/test_fixed_step.cpp).
- **Check:** With an empty accumulator, how many updates does a 1/30-second frame
  produce? Why reset accumulated time when play is interrupted?

<details>
<summary>Answer</summary>

At the default 1/60-second step, `FixedStep::advance` calls the update twice, each
with 1/60 second. `runGameUpdates` calls `FixedStep::reset` when simulation is blocked
or play is interrupted, so pending time cannot cause a burst of updates on resuming.

</details>

### Presentation

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

<details>
<summary>Answer</summary>

`appendActors` fills the draw command's `position`, `size`, and `rotationRadians`
from `placeActorSprite`, converting the position with `worldToScreen`. Draw order
comes from the order commands are appended to `scene.sprites` in `buildRenderScene`;
actors within that group follow `world.actors()` order.

</details>

### Level composition and JSON loading

- In [`level_composition.cpp`](../app/game/level_composition.cpp), read `composeGameLevel`:
  level data and catalog definitions become a map and world.
- In [`level_data.cpp`](../app/content/level_data.cpp), start with `loadLevelData` and
  `parseLevelData`. Follow `jsonLevelData`: `jsonLegends` parses typed templates,
  `jsonExplicitPlacements` reads positioned objects, then `placeMapObjects` copies
  templates at marked cells. The placement pass works with C++ values; JSON is read once.
- **Starting test:** "A level composes an actor from its catalog definition" in
  [`test_level_composition.cpp`](../tests/app/game/test_level_composition.cpp).
- **Check:** Which data comes from the catalog, and which comes from the level placement?

<details>
<summary>Answer</summary>

Catalog definitions supply shared properties such as body size, movement settings,
and art. Level placements supply the definition name, position, and optional patrol
or exit settings. `composeGameLevel` resolves the names and combines those values
into the map and world; `Game::startLevel` inserts the player at the level's spawn.

</details>

### CI, formatting, and static analysis

- Read the [quality commands](../README.md#continuous-integration), then skim
  [`ci.yml`](../.github/workflows/ci.yml) for the build, test, and quality jobs.
- In [`Quality.cmake`](../cmake/Quality.cmake), find the formatting targets, `tidy`,
  and `header_self_containment`. Follow formatting into
  [`format.py`](../tools/format.py), starting at `files_for` and `commands`.
- **Starting test:** `test_check_mode_never_requests_edits_and_includes_lint` in
  [`test_format.py`](../tools/test_format.py). Run the tool tests from the repository
  root with `python3 -m unittest discover -s tools -p 'test_*.py'`.
- **Check:** How does checking formatting differ from applying it? What does the
  header self-containment target verify?

<details>
<summary>Answer</summary>

`commands` in `format.py` selects checks rather than edits when `--check` is set;
Python checking also includes lint. `header_self_containment` compiles each public
header on its own, checking that it supplies the includes it needs. These checks
complement the gameplay tests.

</details>

### Cover fading

- Read [Cover](ARCHITECTURE.md#cover) to distinguish NPC sight from screen visibility.
- In [`cover_fade.cpp`](../src/render/cover_fade.cpp), read `updateCoverFades`, then
  `visibility`, `playerTarget`, and `fadeTowards`. Follow the resulting
  `screenVisibility` into `appendActors` in
  [`render_scene.cpp`](../src/render/render_scene.cpp).
- **Starting test:** "An NPC fades in over the fade time when the player joins its patch"
  in [`test_cover_fade.cpp`](../tests/render/test_cover_fade.cpp).
- **Check:** Does cover visibility change instantly? How is the concealed player
  drawn differently from an NPC?

<details>
<summary>Answer</summary>

`fadeTowards` sets an unset visibility to its target immediately; later changes
move toward the target over time. `appendActors` uses NPC visibility for opacity,
while the player's visibility controls shading. `playerTarget` exposes the player
when an NPC sees them or a recent shot is within the reveal window.

</details>

### Climbing and flying navigation

- Return to [`actor_navigation.cpp`](../src/navigation/actor_navigation.cpp): follow
  `findActorPath` into `findFlyingPath`, then read `findPlatformerPath` for choosing
  a starting floor, wall, or ceiling location.
- In [`path_follower.cpp`](../src/navigation/path_follower.cpp), read `followFlyingPath`
  and `followClimbStep`. Compare steering toward a waypoint with approaching a
  surface and replaying a climb's recorded inputs.
- **Starting tests:** "A flying path crosses open cells around a wall", then
  "One path crosses a floor, a wall, a ceiling and another floor" in
  [`test_actor_navigation.cpp`](../tests/navigation/test_actor_navigation.cpp).
- **Check:** Why does a climber need a surface as well as a cell? Does flying use the
  platformer's simulated jump connections?

<details>
<summary>Answer</summary>

A floor, wall, and ceiling in one cell are different resting places for a climber's
body. `findPlatformerPath` searches those locations with the actor's traversal
profile. `findFlyingPath` instead searches neighbouring open cells; it does not use
simulated platformer jump connections. `followFlyingPath` steers through its waypoints.

</details>

### The connection table

- Read [The connection table](ARCHITECTURE.md#the-connection-table) for traversal
  profiles and the tile footprints recorded by connection simulations.
- In [`platformer_connection_table.cpp`](../src/navigation/platformer_connection_table.cpp),
  read `PlatformerConnectionTable::prepare`, then `applyRecordedTileBreaks` and
  `connections`. Follow `prepareNavigation` in
  [`actor_navigation.cpp`](../src/navigation/actor_navigation.cpp) for level startup.
- **Starting test:** "A break rebuilds only the cells whose footprint holds it" in
  [`test_platformer_connection_table.cpp`](../tests/navigation/test_platformer_connection_table.cpp).
- **Check:** When can actors share connections? Does breaking one tile rebuild every
  cell's connections?

<details>
<summary>Answer</summary>

Actors with equal traversal profiles share a table: body size, movement settings,
simulation step, and optional climbing settings must match. `prepare` builds a
profile's connections. `applyRecordedTileBreaks` rebuilds only cells whose stored
simulation footprints contain a newly broken tile.

</details>

### OpenGL setup and shaders

- In [`sprite_renderer.cpp`](../app/graphics/sprite_renderer.cpp), read
  `createShaderProgram`, then `SpriteRenderer::render`. Trace one draw command into
  vertex positions, atlas texture coordinates, and shader inputs.
- Read [`display_viewport.cpp`](../app/graphics/display_viewport.cpp) for fitting
  the internal image into the framebuffer. Keep the OpenGL resource setup separate
  from the game rules that produced the draw commands.
- **Starting test:** "The display viewport uses the largest integer scale" in
  [`test_display_viewport.cpp`](../tests/app/graphics/test_display_viewport.cpp).
- **Manual check:** Run the game and resize the window. Check sprite placement,
  scaling, and letterboxing; the viewport test does not execute OpenGL or shaders.
- **Check:** What image does the renderer draw first, and how does it fit that image
  into the window?

<details>
<summary>Answer</summary>

`SpriteRenderer::render` first draws the scene into the 320 × 180 internal framebuffer.
It then copies that image into the display viewport. `makeDisplayViewport` chooses
an integer scale and centres the image, leaving letterbox space where needed.
`createShaderProgram` compiles and links the shaders used to draw sprites.

</details>

Once the route above makes sense, use [ARCHITECTURE.md](ARCHITECTURE.md) as the
detailed reference for boundaries, ownership, conventions, and design trade-offs.
