# Glossary

The words the code and documents use, and what each one means. Look a word up here when
its meaning in the code is not clear. Terms are grouped by the part of the engine they
belong to.

## The engine

| Term          | Meaning                                                                                                                                                                                                                         |
| ------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Actor         | Anything in the world that moves or acts: the player, an enemy, a projectile's owner. An actor is a set of components, not a class in a hierarchy. See [`actor.hpp`](../include/simple_platformer/actor/actor.hpp).             |
| Component     | One part of an actor, such as its body, its movement, its health or its brain. Most are optional; what an actor has decides what it can do.                                                                                     |
| World         | Owns the actors, pickups, projectiles, level exit and world clock for one level. See [`world.hpp`](../include/simple_platformer/world/world.hpp).                                                                               |
| System        | A function that updates one kind of component for every actor that has it, such as movement, attacks or senses. `updateWorldSimulation` runs them in a fixed order.                                                             |
| Step, tick    | One fixed update of the simulation, 1/60 of a second. The two words mean the same thing.                                                                                                                                        |
| `deltaTime`   | The length of the step a system is running, in seconds.                                                                                                                                                                         |
| Intentions    | What an actor wants to do this step: move, jump, attack, aim, climb. The player's come from the keyboard and mouse, an NPC's from its brain. Movement and combat read only intentions. See `InputIntentions`.                   |
| Body          | An actor's box in the world and its velocity. See [`body.hpp`](../include/simple_platformer/physics/body.hpp).                                                                                                                  |
| AABB          | An axis-aligned bounding box: a rectangle placed by its top-left corner, used for every body and for collision.                                                                                                                 |
| Feet          | The middle of a body's bottom edge, where it stands. Levels place actors by their feet.                                                                                                                                         |
| Tile, cell    | A tile is a square of the map with its own rules (blocks movement, blocks sight, climbable, breakable). A cell is a tile's position on the grid, written `{column, row}`.                                                       |
| Tile map      | The level's grid of tiles. See [`tile_map.hpp`](../include/simple_platformer/world/tile_map.hpp).                                                                                                                               |
| World clock   | The world's running time in seconds, advanced once at the start of every step.                                                                                                                                                  |
| Timer         | A `float` on a component that counts down to zero or up to a length, such as `coyoteRemaining`.                                                                                                                                 |
| Stamp         | The world clock's time when something happened, such as `lastDamageTimeSeconds`. Readers ask how long ago it was.                                                                                                               |
| World request | A change a system asks for during a step, such as removing an actor, applied once the systems that might be reading the lists have finished. See [`world_requests.hpp`](../include/simple_platformer/world/world_requests.hpp). |

## Movement

| Term        | Meaning                                                                                                                  |
| ----------- | ------------------------------------------------------------------------------------------------------------------------ |
| Platformer  | An actor with platformer movement: it walks, jumps and falls under gravity. The player is one.                           |
| Flyer       | An actor with flying movement: it moves in any direction and ignores gravity.                                            |
| Climber     | A platformer that can also hold climbable walls and ceilings. See `SurfaceClimb`.                                        |
| Surface     | Where a body rests in a cell: its floor, its left or right wall, or its ceiling. Only a climber uses walls and ceilings. |
| Grounded    | Standing on something solid this step.                                                                                   |
| Coyote time | A short window after walking off a ledge in which a jump still works.                                                    |
| Jump buffer | A short window in which a jump pressed just before landing still happens on landing.                                     |
| Traversal   | A kind of move between two places: walk, fall, jump, climb or fly.                                                       |

## NPC behaviour

| Term       | Meaning                                                                                                           |
| ---------- | ----------------------------------------------------------------------------------------------------------------- |
| NPC        | An actor with a brain, which decides its intentions instead of a player.                                          |
| Senses     | How far an NPC notices and how long it remembers. Tuned in content.                                               |
| Perception | What an NPC saw or heard in the latest sensing update. Replaced every update.                                     |
| Brain      | What an NPC knows and decides with between updates: its state, its target and its memory of where the target was. |
| Target     | The actor an NPC is after, usually the player. Not the same as a goal.                                            |
| Fact       | A yes-or-no answer about an NPC this update, such as `targetVisible` or `hasPatrol`, that its transitions test.   |
| State      | What an NPC is doing, such as Patrol, Chase or Bite.                                                              |
| Tactic     | The built-in policy for choosing states: Pursuer closes in, KeepDistance keeps its range.                         |
| Activity   | What a state does each step. A built-in activity is written in C++; a scripted activity is written in Lua.        |
| Patrol     | Two points an NPC walks or flies between while it has no target.                                                  |
| Noise      | An event other actors can hear, such as a landing or a shot.                                                      |

## Navigation

| Term              | Meaning                                                                                                                                      |
| ----------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| Goal              | The point a path heads for. It need not be somewhere the actor can be.                                                                       |
| Location          | A place the search works with: a cell and a surface.                                                                                         |
| Connection        | One move from a location to a nearby one, with its traversal, its cost and any recorded inputs.                                              |
| Search            | The A\* search in `route_search` that finds the cheapest route.                                                                              |
| Expand            | For the search, to look at every connection leaving a location.                                                                              |
| Heuristic         | A guess of the cost still to go. It must never guess more than the real cost.                                                                |
| Route             | What the search returns: a start location and the steps from it, in cells and surfaces.                                                      |
| Path              | What navigation gives an actor: a route turned into waypoints, in world coordinates.                                                         |
| Waypoint          | Where the body's feet rest at the end of one step of a path, with how the step is travelled.                                                 |
| Destination       | Where one step of a route or path leads.                                                                                                     |
| Find              | To produce a path for an actor, with `findActorPath`. Finding runs a search.                                                                 |
| Follow            | To turn a path into intentions, step by step, with the path follower.                                                                        |
| Traversal profile | Everything a platformer's connections depend on: body size, movement, climbing and the step. Actors with the same profile share connections. |
| Connection cache  | Where the connections for each cell and profile are kept between searches.                                                                   |
| Fill              | The background work that builds the cache a little at a time, each step.                                                                     |
| Deferred          | A search that stopped because the cache did not hold a cell it needed yet. The caller tries again later.                                     |

## Presentation and content

| Term                | Meaning                                                                                                                  |
| ------------------- | ------------------------------------------------------------------------------------------------------------------------ |
| Render scene        | The list of plain draw commands built from the world each frame, before anything touches OpenGL.                         |
| Internal resolution | The 320 by 180 image the game draws, scaled to fit the window.                                                           |
| Atlas               | The one image every sprite is cut from.                                                                                  |
| Cover               | Tiles that block sight but can be walked into, such as grass. Whatever stands in cover is hidden from anyone outside it. |
| Catalog             | A JSON file of named definitions, such as `actors.json` or `items.json`.                                                 |
| Definition          | One named entry in a catalog, which levels refer to by name.                                                             |
| Legend              | The part of a level file that says what each character in its map rows means.                                            |
