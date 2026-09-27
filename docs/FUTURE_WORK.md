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
The spider still needs wall and ceiling movement, collision contacts, surface navigation,
and path following in C++. Build and test those capabilities without Lua first. A
pounce would also need an engine-owned movement request. Lua can then choose when to
patrol, chase, or pounce; C++ executes those moves.

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

## Closest reachable chase destination

Chase currently selects the nearest standable cell to the target's last known feet;
that cell can be unreachable. A future search could try candidates in deterministic
nearest-first order and return both the reachable path and its destination, avoiding
a second search for the chosen cell. Use path cost to break ties and the existing
repath delay to bound extra work. The NPC would wait at the closest reachable endpoint
while remaining in Chase. The search must use remembered position, never the hidden
player's current position.
