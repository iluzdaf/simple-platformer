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
