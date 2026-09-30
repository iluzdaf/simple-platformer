# Future work

These are proposals, not implemented features or requirements for ordinary project work.
For the current design, use [ARCHITECTURE.md](ARCHITECTURE.md); for editing game data,
use [CONTENT.md](CONTENT.md).

## Level authoring tools

Editing JSON remains useful because the stored level data is visible and reviewable,
but counting columns in a wide tile map makes object placement cumbersome.

The first step should be a read-only level visualiser. It can load the existing JSON
through the normal parser and display row and column rulers together with symbols for
the player, actors, pickups, exits, and patrol points. It should report the same
validation errors as the game and must not introduce another level format.

A later visual editor can let a user paint tiles and place objects, then write the same
validated JSON consumed by the example game. The loader should still produce
`LevelData`, and composition should still create the core's map and world.
Simulation must not depend on the editor or JSON, so handwritten and tool-generated
levels remain equivalent.

## Surface movement and a scripted spider

The rat and boar already use Lua activities over C++ sensing, movement, navigation,
and combat. See [NPC behaviour](ARCHITECTURE.md#npc-behaviour) for the current boundary.
The engine has opt-in wall and ceiling climbing on explicitly marked tiles, routes one
path across floors, walls, and ceilings, and turns a climber's sprite onto the surface
it holds. The spider uses all of this: its Lua activities patrol and pursue over walls
and ceilings, and C++ follows the routes and bites.

Two pieces remain. A climber turns only inside corners, so it cannot go over the top of
a free-standing wall or round a ledge; outside corners need both the climbing movement
and navigation. A pounce would need an engine-owned movement request, which Lua could
then choose when to use; C++ executes the move.

## Optional movement abilities

`PlatformerMovement` is the shared baseline for ground actors. Add abilities such as
double jump, dash, wall slide, or wall jump as optional actor components when a feature
needs them. Keep their configuration and runtime state separate from ordinary walking.

If several abilities coexist, use an explicit update order:

1. Select an ability and produce movement modifiers.
2. Apply ordinary movement and collision with those modifiers.
3. Update ability state from collision contacts, such as landing or hitting a wall.

Resolve competing abilities in visible policy code, with a documented priority. Test
each ability on its own and cover interactions that change the result. Introduce this
phase with a real ability rather than a general callback framework in advance.
