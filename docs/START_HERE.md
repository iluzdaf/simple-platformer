# Start Here

This is the recommended first route through Simple Platformer. It follows one player
update from the application into the engine, then follows the resulting world back to
the renderer. You do not need to understand every subsystem before changing the game.

Build and run the project using the instructions in [README.md](../README.md) before
starting the tour. Keep a matching test file open beside each implementation file;
the tests often provide the smallest examples of how a subject is meant to be used.

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
   build and open the frame panel with F1. See
   [Profiling](../README.md#macos-configure-build-and-test) for the preset.

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

Use this section to find the code involved in common project requirements. Platformer
mechanics and enemy behaviour share the same movement, collision, combat, and input
systems: an NPC produces the same `InputIntentions` that the application produces for
the player.

### Platformer gameplay requirements

For movement mechanics and other platformer gameplay, start with this path:

```text
keyboard and mouse
  -> InputState
  -> InputIntentions
  -> PlatformerMovement
  -> collision
  -> Body
```

A practical route through the implementation is:

1. Trace an ordinary run and jump through
   [`input_state.cpp`](../src/input/input_state.cpp),
   [`platformer_movement.cpp`](../src/movement/platformer_movement.cpp), and
   [`collision.cpp`](../src/physics/collision.cpp).
2. Tune speed, acceleration, braking, gravity, and jump configuration in
   [`actors.json`](../assets/actors.json), then observe
   how those values change the feel of the example game.
3. Read the tests for the existing variable-height jump, coyote-time, and jump-buffer
   rules before changing them.
4. Add one focused movement ability, such as a double jump, dash, wall slide, or wall
   jump. Keep the rule in the movement layer and cover it with tests before adding its
   animation or effects.
5. Turn the mechanics into a game by composing actors, authoring JSON levels, adding
   pickups or combat rules, and providing animation and HUD feedback.

The engine currently provides the ordinary platformer baseline. It does not already
contain a generic movement-ability framework. The
[movement extension recipe](ARCHITECTURE.md#adding-a-movement-ability) explains where
a movement feature belongs; the more scalable optional-component design remains a
clearly labelled [future direction](FUTURE_WORK.md#optional-movement-abilities).

### Enemy behaviour requirements

For finite-state behaviour, sensing, and pathfinding requirements, follow this path:

```text
senses and memory
  -> finite-state decision
  -> pathfinding and path following
  -> InputIntentions
  -> the same movement and combat systems
```

A practical route through the implementation is:

1. Trace the explicit `NpcState` enum, its transitions in
   [`npc_transitions.cpp`](../src/npc/npc_transitions.cpp), and the state branches in
   [`npc_system.cpp`](../src/npc/npc_system.cpp).
2. Change `noticeDistance`, `standoffDistance`, `targetMemoryDuration` and
   `searchDuration` in an actor definition's `senses` settings in [`actors.json`](../assets/actors.json), using the
   debug overlay to observe visible targets, remembered positions, patrol points,
   destinations, and paths.
3. Add one state such as Guard or Recover and test its transitions separately from
   movement. Optionally, write the same rules as a machine in
   [`machines.json`](../assets/machines.json) and give it to an actor, to compare the
   switch with the data.
4. Read generic lowest-cost search and flying navigation before studying simulated
   platformer jumps.
5. Create an enemy with a deliberate combination of movement, senses, tactic, state
   rules, navigation, attack, and animation.

The [NPC-state recipe](ARCHITECTURE.md#adding-an-npc-state) and
[enemy-composition recipe](ARCHITECTURE.md#creating-a-new-enemy) list the files and
boundaries involved. More general brain tactics are future work and are not required
to extend the current NPC behaviour.

Movement work does not require reading NPC or navigation code. Enemy work builds on
the ordinary movement and collision path, so those systems are useful context when an
enemy does not move as intended.

## Recommended reading route

### 1. Find the outside of the program

Start with [`app/main.cpp`](../app/main.cpp), then skim
[`app/application.cpp`](../app/application.cpp). The application owns the window, input,
fixed-step loop, UI, and graphics setup. Do not worry about the OpenGL details yet.

### 2. See what the game coordinates

Read [`app/game/game.hpp`](../app/game/game.hpp) and
[`app/game/game.cpp`](../app/game/game.cpp). `Game` is the seam between the application
and the engine: it passes input into the simulation, updates presentation state, changes
levels, and builds a scene for rendering. What it and `GameLevel` own is listed under
[Application folders](ARCHITECTURE.md#application-folders).

Then open [`assets/levels.json`](../assets/levels.json). It chooses the
starting level and maps level IDs to filenames, so level files can be freely renamed.
[`level_catalog.cpp`](../app/content/level_catalog.cpp) validates that catalogue and
resolves its filenames. Follow its first entry into
[`assets/level_1.json`](../assets/level_1.json), which contains the map and
placements for that level. Follow that data into
[`level_data.cpp`](../app/content/level_data.cpp), which validates the JSON,
and then [`actor_catalog.cpp`](../app/content/actor_catalog.cpp) and
[`actor_definition.cpp`](../app/content/actor_definition.cpp), which load named actor settings
from `actors.json` and compose C++ actors.
[`level_composition.cpp`](../app/game/level_composition.cpp) brings the catalogues and
placements together into a `GameLevel`.
[`item_catalog.cpp`](../app/content/item_catalog.cpp) and
[`pickup_catalog.cpp`](../app/content/pickup_catalog.cpp) load inventory items and world
pickup definitions from `items.json` and `pickups.json`.
[`exit_catalog.cpp`](../app/content/exit_catalog.cpp) loads exit bounds and sprites from
`exits.json`; positions and completion settings belong to each level.
These are game-content concerns, not general engine behaviour.
[CONTENT.md](CONTENT.md) is the complete reference when you are ready to edit or add
levels.

### 3. Learn the core data model

Read these headers first:

- [`actor.hpp`](../include/simple_platformer/actor/actor.hpp) shows an actor assembled
  from optional components;
- [`world.hpp`](../include/simple_platformer/world/world.hpp) shows what the world owns;
- [`body.hpp`](../include/simple_platformer/physics/body.hpp) shows the physical state;
- [`input_state.hpp`](../include/simple_platformer/input/input_state.hpp) shows the common
  intentions used by the player and NPCs.

The important idea is composition: the player, zombie, bat, and ranged NPC are not
different subclasses. They are actors with different combinations of data. The full
recipe is in [Actor composition](ARCHITECTURE.md#actor-composition).

### 4. Follow one simulation tick

Read [`world_simulation.cpp`](../src/world/world_simulation.cpp). It is the short,
authoritative list of gameplay systems and their order. From there, follow only the
system relevant to the feature you are studying.

For the player movement path, a useful order is:

1. [`input_state.cpp`](../src/input/input_state.cpp)
2. [`platformer_movement.cpp`](../src/movement/platformer_movement.cpp)
3. [`collision.cpp`](../src/physics/collision.cpp)
4. [`tile_map.cpp`](../src/world/tile_map.cpp)

Read the matching files under `tests/` beside them. For example,
[`test_platformer_movement.cpp`](../tests/movement/test_platformer_movement.cpp) isolates
movement rules, while [`test_collision.cpp`](../tests/physics/test_collision.cpp)
isolates collision rules.

### 5. Follow presentation separately

Read these after the movement loop:

1. [`render_scene.cpp`](../src/render/render_scene.cpp) converts world state into plain
   sprite draw commands;
2. [`camera.cpp`](../src/render/camera.cpp) follows the player and converts world space to
   screen space;
3. [`presentation.cpp`](../src/render/presentation.cpp) runs the presentation systems
   after the simulation: [`animation_system.cpp`](../src/render/animation_system.cpp)
   selects and advances actor animation clips, and
   [`cover_fade.cpp`](../src/render/cover_fade.cpp) eases what the player can see of
   NPCs and pickups in grass; pickup bobbing and timed feedback are calculated from world
   state in `render_scene.cpp`;
4. [`sprite_renderer.cpp`](../app/graphics/sprite_renderer.cpp) submits the finished draw
   commands to OpenGL.

The first three can be understood and tested without knowing OpenGL.

### 6. Add NPC behaviour and combat

NPCs use the same actor movement and attack systems as the player. Their brain produces
intentions instead of reading a keyboard. Follow this route:

1. [`npc_senses.cpp`](../src/npc/npc_senses.cpp)
2. [`npc_transitions.cpp`](../src/npc/npc_transitions.cpp)
3. [`npc_system.cpp`](../src/npc/npc_system.cpp)
4. [`attack_system.cpp`](../src/combat/attack_system.cpp)
5. [`projectile_system.cpp`](../src/combat/projectile_system.cpp)
6. [`lifecycle.cpp`](../src/actor/lifecycle.cpp)

The enum-and-switch NPC state machine is intentionally explicit. The bite and ranged
attacks have different gameplay data, but both use the same primary-attack intention.

### 7. Read navigation last

Navigation is the most advanced part of the repository. First understand the NPC state
machine and ordinary movement. Then read:

1. [`path_search.cpp`](../src/navigation/path_search.cpp) for the generic lowest-cost
   search;
2. [`flying_navigation.cpp`](../src/navigation/flying_navigation.cpp) for the simplest
   neighbour policy;
3. [`path_follower.cpp`](../src/navigation/path_follower.cpp) for turning a path into
   intentions;
4. [`platformer_navigation.cpp`](../src/navigation/platformer_navigation.cpp) for the
   advanced walk, fall, and simulated-jump policy, and the search that reads a cache;
5. [`connection_cache.cpp`](../src/navigation/connection_cache.cpp) for what platformer
   searches remember, and how a broken tile or the fill phase changes it.

The platformer navigation code reuses the real movement and collision functions. It is
valuable, but it is not the best first example of the engine's general style. With the
overlay open, N shows the cache's cells for each NPC body in turn and B breaks the tile
under the cursor, so what the cache keeps and what a break drops can be watched in
the game. [Navigation](ARCHITECTURE.md#navigation) in the architecture document
explains each piece in the order the code builds them up.

### 8. Complete the level loop

Finally, read the small inventory and world-object subjects:

- [`inventory.cpp`](../src/inventory/inventory.cpp)
- [`item_use.cpp`](../src/inventory/item_use.cpp)
- [`pickup.cpp`](../src/world/pickup.cpp)
- [`level_exit.cpp`](../src/world/level_exit.cpp)
- [`world_requests.cpp`](../src/world/world_requests.cpp)

These show automatic pickup, deferred world changes, item use, and level completion.

## What to skip on a first reading

It is safe to return later to:

- OpenGL setup and shader details in `app/graphics`;
- ImGui layout code in `app/ui` and `app/debug`;
- simulated platformer navigation;
- atlas coordinates and clip timings in `assets/animations.json`;
- CI, formatting, and static-analysis targets.

Once the route above makes sense, use [ARCHITECTURE.md](ARCHITECTURE.md) as the
detailed reference for boundaries, ownership, conventions, and design trade-offs.
