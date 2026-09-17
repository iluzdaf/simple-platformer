# Start Here

This is the recommended first route through Simple Platformer. It follows one player
update from the application into the engine, then follows the resulting world back to
the renderer. You do not need to understand every subsystem before changing the game.

Build and run the project using the instructions in [README.md](README.md) before
starting the tour. Keep a matching test file open beside each implementation file;
the tests often provide the smallest examples of how a subject is meant to be used.

## The big picture

The outer flow is:

```text
main.cpp
  -> runApplication()
  -> ExampleGame::update()
  -> updateWorldSimulation()
  -> gameplay systems
```

Rendering follows a separate path:

```text
ExampleGame::buildScene()
  -> RenderScene (plain draw data)
  -> SpriteRenderer (OpenGL)
```

This separation lets most game behaviour run in tests without opening a window.

## Recommended reading route

### 1. Find the outside of the program

Start with [`app/main.cpp`](app/main.cpp), then skim
[`app/application.cpp`](app/application.cpp). The application owns the window, input,
fixed-step loop, UI, and graphics setup. Do not worry about the OpenGL details yet.

### 2. See what the example game coordinates

Read [`app/game/example_game.hpp`](app/game/example_game.hpp) and
[`app/game/example_game.cpp`](app/game/example_game.cpp). `ExampleGame` owns the current
`GameLevel` and camera controller. `GameLevel` keeps the level ID, map, world, and player
spawn together. `ExampleGame` passes input into the simulation, updates presentation
state, changes levels, and builds a scene for rendering.

Then open [`assets/levels/levels.json`](assets/levels/levels.json). It chooses the
starting level and maps level IDs to filenames, so level files can be freely renamed.
[`level_catalog.cpp`](app/game/level_catalog.cpp) validates that catalog and
resolves its filenames. Follow its first entry into
[`assets/levels/level_1.json`](assets/levels/level_1.json), which contains the map and
placements for that level. Follow that data into
[`example_level_data.cpp`](app/game/example_level_data.cpp), which validates the JSON,
and then [`example_content.cpp`](app/game/example_content.cpp), which turns known names
such as `zombie` into composed C++ actors. These are game-content concerns, not general
engine behaviour. The
[`Data-driven level boundary`](ARCHITECTURE.md#data-driven-level-boundary) section is
the complete reference when you are ready to edit or add levels.

### 3. Learn the core data model

Read these headers first:

- [`actor.hpp`](include/simple_platformer/actor/actor.hpp) shows an actor assembled
  from optional components;
- [`world.hpp`](include/simple_platformer/world/world.hpp) shows what the world owns;
- [`body.hpp`](include/simple_platformer/physics/body.hpp) shows the physical state;
- [`input_state.hpp`](include/simple_platformer/input/input_state.hpp) shows the common
  intentions used by the player and NPCs.

The important idea is composition: the player, zombie, bat, and ranged NPC are not
different subclasses. They are actors with different combinations of data. The full
recipe is in [Actor composition](ARCHITECTURE.md#actor-composition).

### 4. Follow one simulation tick

Read [`world_simulation.cpp`](src/world/world_simulation.cpp). It is the short,
authoritative list of gameplay systems and their order. From there, follow only the
system relevant to the feature you are studying.

For the player movement path, a useful order is:

1. [`input_state.cpp`](src/input/input_state.cpp)
2. [`platformer_movement.cpp`](src/movement/platformer_movement.cpp)
3. [`collision.cpp`](src/physics/collision.cpp)
4. [`tile_map.cpp`](src/world/tile_map.cpp)

Read the matching files under `tests/` beside them. For example,
[`test_platformer_movement.cpp`](tests/movement/test_platformer_movement.cpp) isolates
movement rules, while [`test_collision.cpp`](tests/physics/test_collision.cpp)
isolates collision rules.

### 5. Follow presentation separately

Read these after the movement loop:

1. [`render_scene.cpp`](src/render/render_scene.cpp) converts world state into plain
   sprite draw commands;
2. [`camera.cpp`](src/render/camera.cpp) follows the player and converts world space to
   screen space;
3. [`animation_system.cpp`](src/render/animation_system.cpp) selects and advances
   actor animation clips;
4. [`sprite_renderer.cpp`](app/graphics/sprite_renderer.cpp) submits the finished draw
   commands to OpenGL.

The first three can be understood and tested without knowing OpenGL.

### 6. Add NPC behaviour and combat

NPCs use the same actor movement and attack systems as the player. Their brain produces
intentions instead of reading a keyboard. Follow this route:

1. [`npc_senses.cpp`](src/npc/npc_senses.cpp)
2. [`npc_system.cpp`](src/npc/npc_system.cpp)
3. [`attack_system.cpp`](src/combat/attack_system.cpp)
4. [`projectile_system.cpp`](src/combat/projectile_system.cpp)
5. [`lifecycle.cpp`](src/actor/lifecycle.cpp)

The enum-and-switch NPC state machine is intentionally explicit. The bite and ranged
attacks have different gameplay data, but both use the same primary-attack intention.

### 7. Read navigation last

Navigation is the most advanced part of the repository. First understand the NPC state
machine and ordinary movement. Then read:

1. [`path_search.cpp`](src/navigation/path_search.cpp) for the generic lowest-cost
   search;
2. [`flying_navigation.cpp`](src/navigation/flying_navigation.cpp) for the simplest
   neighbour policy;
3. [`path_follower.cpp`](src/navigation/path_follower.cpp) for turning a path into
   intentions;
4. [`platformer_navigation.cpp`](src/navigation/platformer_navigation.cpp) for the
   advanced walk, fall, and simulated-jump policy.

The platformer navigation code reuses the real movement and collision functions. It is
valuable, but it is not the best first example of the engine's general style.

### 8. Complete the level loop

Finally, read the small inventory and world-object subjects:

- [`inventory.cpp`](src/inventory/inventory.cpp)
- [`item_use.cpp`](src/inventory/item_use.cpp)
- [`pickup.cpp`](src/world/pickup.cpp)
- [`level_exit.cpp`](src/world/level_exit.cpp)
- [`world_requests.cpp`](src/world/world_requests.cpp)

These show automatic pickup, deferred world changes, item use, and level completion.

## What to skip on a first reading

It is safe to return later to:

- OpenGL setup and shader details in `app/graphics`;
- ImGui layout code in `app/ui` and `app/debug`;
- simulated platformer navigation;
- atlas coordinates in `example_animations.cpp`;
- CI, formatting, and static-analysis targets.

Once the route above makes sense, use [ARCHITECTURE.md](ARCHITECTURE.md) as the
detailed reference for boundaries, ownership, conventions, and design trade-offs.
