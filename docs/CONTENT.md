# Content and Level Format

This is the authoring reference for the JSON under `assets`. It covers the level catalogue,
each shared definition file, the map and placement syntax, and the loader vocabulary in
`app/content`.

The files select and place known game concepts rather than defining new engine behaviour.
For example, `"definition": "zombie"` in an actor placement selects a named definition in
`actors.json`. Definitions select and configure supported components; behaviour
implementations remain C++.

Two rules hold for every file described here. Unknown fields are rejected, so a
misspelling is reported rather than ignored. Every definition is checked in full, so a
mistake in an entry the level never places is still reported.

[ARCHITECTURE.md](ARCHITECTURE.md) explains why this boundary exists and what the engine
does with the loaded data. [README.md](../README.md) covers building and running.

## Level catalogue

The [content-file guide](#content-files-at-a-glance) below lists the shared definitions
used alongside this catalogue.

`assets/levels.json` selects the starting level and assigns stable numeric level
IDs to files:

```json
{
  "startLevel": 1,
  "levels": [
    {"number": 1, "file": "level_1.json"},
    {"number": 2, "file": "level_2.json"},
    {"number": 3, "file": "level_3.json"}
  ]
}
```

- `number` is a positive, unique level ID.
- `file` is a path relative to the catalogue's directory.
- `startLevel` names one of the catalogue entries.
- An exit's `nextLevel` refers to a level ID in the catalogue.

Each catalogue entry assigns a level ID to a level file. The referenced file contains that
level's map and object placements. Students can rename, add, or remove level files by
updating the catalogue without changing C++.

## Content files at a glance

Shared catalogues sit beside `levels.json` in `assets`:

| File | What to change here | Loader or composition code |
| --- | --- | --- |
| [`levels.json`](../assets/levels.json) | Starting level and level ID-to-file mapping | [`level_catalog.cpp`](../app/content/level_catalog.cpp) |
| A level file, such as [`level_1.json`](../assets/level_1.json) | Map rows, legends, spawns, patrols, pickups, and exit settings | [`level_data.cpp`](../app/content/level_data.cpp) |
| [`tiles.json`](../assets/tiles.json) | Tile artwork, movement/sight properties, and what a tile breaks into | [`tile_catalog.cpp`](../app/content/tile_catalog.cpp) |
| [`actors.json`](../assets/actors.json) | Player definition, actor capabilities, and tuning | [`actor_catalog.cpp`](../app/content/actor_catalog.cpp), [`actor_definition.cpp`](../app/content/actor_definition.cpp) |
| [`animations.json`](../assets/animations.json) | Named animation sets, frame rectangles, timing, and looping | [`animation_catalog.cpp`](../app/content/animation_catalog.cpp) |
| [`items.json`](../assets/items.json) | Inventory names, icons, stacking, and effect settings | [`item_catalog.cpp`](../app/content/item_catalog.cpp) |
| [`pickups.json`](../assets/pickups.json) | World pickup quantities, bounds, and optional sprites | [`pickup_catalog.cpp`](../app/content/pickup_catalog.cpp) |
| [`exits.json`](../assets/exits.json) | Exit bounds and sprites | [`exit_catalog.cpp`](../app/content/exit_catalog.cpp) |

[`level_composition.cpp`](../app/game/level_composition.cpp) combines definitions and placements
into runtime objects. Catalogues and JSON conventions belong to the application;
the core receives C++ values and does not read these files. Shared files are required
even when a particular level uses no pickups or NPCs; item and pickup catalogues can
contain empty definitions objects.

`Game` owns a `GameCatalogs` value loaded once by
[`loadGameCatalogs`](../app/content/game_catalogs.cpp). Tile, animation, actor, item, pickup, and exit
definitions are reused across transitions and restarts. Each level file is loaded
when entering that level; the game does not construct every world at startup.
Restart the game application to reload shared definitions after editing their files.

Actor animation clips are configured in `animations.json`.
See [Actors](#actors), [Exits](#exits), and [Animation](ARCHITECTURE.md#animation) for those boundaries.

## Level files

A minimal level looks like this:

```json
{
  "map": [
    "........",
    "........",
    "########"
  ],
  "playerSpawnCell": [1, 1],
  "actors": [],
  "pickups": [],
  "exit": {
    "definition": "bunker_door",
    "spawnCell": [6, 1]
  }
}
```

Every level requires `map`, one player spawn, `actors`, `pickups`, and `exit`. The actor
and pickup arrays may be empty. An exit without `nextLevel` completes the game.

## Maps and positions

Map rows have the same non-zero length. The default symbols are `.` for empty
and `#` for stone. To use more tile types, supply a level legend:

```json
"tileLegend": { ".": "empty", "#": "stone", "G": "grass", "X": "glass" }
```

An optional `objectLegend` places objects directly in the same map rows:

```json
"objectLegend": {
  "P": { "type": "player" },
  "Z": { "type": "actor", "definition": "zombie" },
  "B": { "type": "actor", "definition": "bat" },
  "S": { "type": "actor", "definition": "zombie_soldier" },
  "K": { "type": "pickup", "definition": "key" },
  "E": { "type": "exit", "definition": "bunker_door", "requirement": { "item": "key", "quantity": 1 } }
}
```

Each occurrence creates a placement using the cell's bottom-centre feet anchor, just
like `spawnCell`, with empty terrain underneath. Symbols must be one character and
cannot appear in both legends. There must be exactly one player and one exit placement,
whether supplied by a marker or explicitly. Repeated NPC and pickup markers create
separate objects. Named pickups obtain their positive quantity from `pickups.json`;
inline item stacks supply it in the placement or legend entry. A pickup falls from
where it is placed until it rests on a tile, and falls again if that tile breaks.

Object entries use the same settings as explicit placements: NPCs can specify a
`patrol`, and exits can specify `requirement`, `consumeItem`, and `nextLevel`.
Patrol endpoints remain absolute positions, not offsets from the marker.
`type` selects the object category: `player`, `actor`, `pickup`, or `exit`.
For actors, exits, and named pickups, `definition` selects an entry in the corresponding
catalogue. A tile legend needs only the definition name because its category is
already established by `tileLegend`.
Do not put `spawnCell` or `spawnFeet` in a legend entry: the marker supplies its position.

Explicit actors and pickups are kept first, followed by markers in row order, left to
right. They are added, not merged or deduplicated. Use explicit placements for overlaps
(such as a zombie inside grass), fractional positions, or individually configured objects.
When using `objectLegend`, empty `actors` and `pickups` arrays may be omitted.
The loader expands markers into ordinary placements and resolves their terrain to empty;
the simulation does not interpret object symbols.

Level parsing errors include the source filename and the field or map cell to inspect.
Map paths use zero-based `map[row][column]` indices; JSON syntax errors report one-based
file lines and byte columns. Duplicate player or exit markers report both placements,
and legend settings are reported by their authored paths, such as `objectLegend.K.definition`.

## Tile definitions

Shared definitions live in `assets/tiles.json`. `tileSize` is the side of one tile in
world pixels, shared by every level that uses the catalogue; the game uses 16. Each
definition requires boolean `blocksMovement` and `blocksSight`. Nonempty tiles also need a
`sprite` with its atlas `position: [x, y]` and no `size`: a tile always fills one cell, so
its region is `tileSize` square. Other sprites in the same atlas carry their own world
size. The `empty` definition must allow movement and sight and is not rendered. Unknown
names and map symbols are rejected during loading. Fixture catalogues supply their own
`tiles.json`.

Changing `tileSize` changes the geometry, not the tuning: jump heights, speeds, and the
navigation reach are pixel values chosen for 16-pixel tiles.

A nonempty tile may add `breaksInto` naming the tile it becomes when broken:

```json
"glass": {
  "sprite": { "position": [144, 192] },
  "blocksMovement": true, "blocksSight": false,
  "breaksInto": "empty"
}
```

The name may be any tile in the same file, including one defined further down, and a
tile cannot break into itself. Omit the field to make a tile unbreakable. Breaking also
needs a weapon that does it, which is `breaksTiles` on a `ranged` weapon in
`actors.json`. Chain the field to wear a tile down in stages, such as glass into
cracked glass into empty.

## Placement coordinates

World coordinates begin at the top-left: positive X points right and positive Y
points down. A cell position is `[column, row]`, also counted from the top-left.

Actors, pickups, exits, and patrol endpoints support two placement forms:

- `spawnCell` places the object's feet at the bottom-centre of a tile cell.
- `spawnFeet` supplies that bottom-centre position directly in world pixels.

The player spawn uses the corresponding names `playerSpawnCell` and `playerSpawnFeet`.
Each placement uses exactly one form. Patrol endpoints use `firstCell` or `firstFeet`,
and `secondCell` or `secondFeet`. Feet provide a stable bottom-centre reference point
and do not imply that an object must stand on the ground.

## Actors

An actor requires `definition` (a name in `actors.json`) and one spawn placement. The supplied
catalogue includes `zombie`, `bat`, and `zombie_soldier`; new definition names do not
require a parser branch. A patrol is optional:

```json
{
  "definition": "zombie",
  "spawnCell": [5, 8],
  "patrol": {
    "firstCell": [5, 8],
    "secondCell": [10, 8]
  }
}
```

Shared definitions live in `actors.json` beside the level catalogue:

```json
{
  "player": "hero",
  "actors": {
    "hero": {
      "bodySize": [12, 20], "team": "player", "health": 3,
      "inventorySlots": 6, "animations": "player", "platformer": {}
    },
    "scout": {
      "bodySize": [12, 8], "team": "enemy", "health": 1,
      "animations": "bat", "spriteAnchor": "center",
      "flying": { "speed": 40 }, "senses": { "noticeDistance": 60 }, "bite": {}
    }
  }
}
```

`actor_definition.hpp` is the C++ configuration boundary; `composeActor` creates fresh
runtime components and calls the engine's `validateActor`. `actor_catalog.cpp` reads
JSON, validates every definition, and resolves names. The
top-level `player` chooses the player definition, which must have health and inventory
for the game's HUD, and must not enable NPC sensing. Level patrols remain per-instance.

Exactly one of `platformer` or `flying` is required. Empty component objects use C++
defaults; omitted optional components are absent. `senses` adds the existing NPC brain,
sensing and path follower together. `health` and `inventorySlots` are positive integers.
Attacks use either `bite` or `ranged`, and require a non-neutral team. There is no
inheritance or arbitrary per-placement override mechanism.

Platformer fields match `PlatformerMovementConfig`; flying exposes `speed`. Sensing
exposes `noticeDistance` and `forgetAfter`. Bite exposes `damage`, `hitboxSize`, `reach`,
`windupDuration`, `activeDuration`, and `recoveryDuration`. Ranged exposes `damage`,
`projectileSize`, `projectileSpeed`, `projectileLifetime`, `shootDuration`,
`recoveryDuration`, `breaksTiles`, and an optional `sprite` object with `position`, `size`,
optional `displaySize`, and optional `anchor`. Sprite coordinates use atlas pixels.
Animation names reference named sets in `animations.json`;
animation frames are not loaded here. `facing` is `left` or `right`, and `spriteAnchor`
is `feet` or `center`.

## Animation sets

Shared sets live in [`animations.json`](../assets/animations.json). Each set contains
`idle`, `move`, `jump`, `fall`, `attack`, and `death` clips. For example, the `move` entry
inside a set:

```json
"move": {
  "frames": [
    {"position": [64, 0], "size": [32, 24]},
    {"position": [128, 0], "size": [32, 24]},
    {"position": [96, 0], "size": [32, 24]},
    {"position": [128, 0], "size": [32, 24]}
  ],
  "frameDuration": 0.16,
  "looping": true
}
```

Frame rectangles use atlas pixels. Order and repeated frames are preserved.
`frameDuration` is seconds per frame; a non-looping clip holds its last frame. Each clip
needs at least one frame and a positive finite duration. All six clips are required
because actor selection can request any of them. Frames within one set share a size,
though different sets can use different sizes. A missing set reference is rejected during
loading.

Actor definitions name a set; they do not carry frames of their own. How the engine picks
a clip at runtime is in [Animation](ARCHITECTURE.md#animation).

## Items and pickups

`items.json` defines inventory items by unique symbolic name. Each definition has a
display `name`, `icon`, and positive `maximumStack`:

```json
{
  "items": {
    "health_potion": {
      "name": "Health potion",
      "icon": { "position": [16, 216], "size": [16, 16] },
      "maximumStack": 5,
      "effect": "heal",
      "effectAmount": 2
    }
  }
}
```

The loader assigns numeric `ItemId` values internally; do not put IDs in JSON.
`Game` loads the item catalogue once and reuses it across level transitions and
restarts, keeping carried inventory consistent. Generated IDs are not persistent asset
identities: changing the catalogue can change them on the next launch. A future saved-game
format should store symbolic names and resolve them when loading. The display `name`
is a UI label and does not need to be unique.

`effect` selects
the C++ behaviour `none` (default, amount zero) or `heal` (positive amount). JSON
configures these behaviours; it does not implement them.

`pickups.json` defines world pickups separately from inventory items:

```json
{
  "pickups": {
    "medicine_box": {
      "item": "health_potion",
      "quantity": 2,
      "bodySize": [16, 16]
    }
  }
}
```

`bodySize` defaults to `[16, 16]`. An optional `sprite` overrides the inventory icon
in the world. Both `icon` and `sprite` use `position` and `size` for their atlas
rectangle, optional `displaySize` (defaults to source size), and optional `anchor`
(`feet` or `center`, default `feet`). The collider remains independent of the sprite.

A level places a named definition using one spawn placement:

```json
{
  "definition": "medicine_box",
  "spawnCell": [4, 8]
}
```

An object legend entry can use the same definition:
`"M": { "type": "pickup", "definition": "medicine_box" }`.
Inline `item` and `quantity` remain a shorthand for a 16-by-16 pickup using its item
icon. A placement must not mix that shorthand with `definition`.
New item and pickup names do not require changes to the level parser.

## Exits

Shared exit appearance lives in `exits.json`:

```json
{
  "exits": {
    "bunker_door": {
      "bodySize": [16, 32],
      "sprite": { "position": [48, 216], "size": [16, 32] }
    }
  }
}
```

Both `bodySize` and `sprite` are required. The sprite uses the same source rectangle,
optional display size, and anchor fields as item icons and pickup sprites.

An exit placement requires a `definition` name and one spawn placement. It may also contain:

- `requirement`, with a name from `items.json` and a positive quantity;
- `consumeItem`, which defaults to `false`;
- `nextLevel`, which refers to a level ID in `levels.json`.

```json
{
  "definition": "bunker_door",
  "spawnCell": [18, 8],
  "requirement": {
    "item": "key",
    "quantity": 1
  },
  "consumeItem": true,
  "nextLevel": 2
}
```

The same settings work in an exit legend entry, with `"type": "exit"` and no spawn
field. Requirements, consumption, and destinations belong to the placement, not the
shared definition: two doors can look the same but lead to different levels.
`composeExit` creates bounds and a sprite; `level_composition.cpp` adds the resolved
item requirement and destination before passing the exit to the World.

## Loading and composition

Level placements do not specify actor, pickup, or exit bounds. Actor definitions own
actor sizes; pickup and exit definitions own their respective sizes.
Composition creates each AABB around its loaded feet position. Their sprites
remain independent, just like actor sprites and bodies.

The JSON dependency stays at the application content boundary.
`level_catalog.cpp` validates the catalogue, and `level_data.cpp` parses a
level into plain `LevelData`, reports invalid fields with their content path, and
retains actor, pickup, exit, and item references. The definition catalogues validate
definitions independently of placement; composition resolves names to runtime values. The
composition step then creates the existing `TileMap`, `World`, actors, pickups, and
exit. Existing construction and level validation remain authoritative.

Parser tests use JSON strings and independent files under `tests/fixtures`, laid out
like `assets/`.
Transition tests use that fixture campaign, not the example game's layout or item values.
Generic content checks load every entry in the example catalogue;
they do not assume particular filenames, a fixed level count, or specific NPCs.

Runtime-only state is never loaded: actor IDs, velocities, current paths, attack timers,
and NPC decisions are created fresh whenever a level starts. Texture IDs are supplied
by the application at runtime. Animation frame regions come from `animations.json`. Projectile sprite regions
are configured in the actor catalogue, not in level placements.

## Naming in `app/content`

Function names in the content module follow a small vocabulary. The verb states what a
function takes, what it returns, and whether it touches the filesystem.

[`content_diagnostics.cpp`](../app/content/content_diagnostics.cpp) supplies the three
functions every error message is built from. It has no JSON dependency, so the plain C++
validators use it as well.

| Name | Takes | Returns | Notes |
| --- | --- | --- | --- |
| `fieldPath` | a path and a key | `"parent.child"` | The diagnostic path, not a filesystem path. |
| `indexPath` | a path and an index | `"parent[2]"` | Used for array elements. |
| `failJson` | a source name, a path, and a message | nothing; it throws | Reports `source: path: message`, omitting either part when it is empty. |

[`content_json.cpp`](../app/content/content_json.cpp) supplies the JSON shape readers on top
of those. A `json` function receives a value; a `read` function finds one by key.

| Name | Takes | Returns | Notes |
| --- | --- | --- | --- |
| `jsonText`, `jsonVector`, `jsonSprite`, ... | a JSON value | the converted value | The caller already holds the value. |
| `readText`, `readVector`, `readName`, ... | an object and a key | the converted value | A missing key is an error. |
| `readOptionalText`, `readOptionalVector`, ... | an object, a key, and a reference | nothing | A missing key keeps the caller's value; a present but invalid one is an error. |
| `checkJsonFields`, `checkJsonObject`, `checkJsonPair` | a JSON value | nothing | Shape assertions. They extract no value. |
| `requiredJsonMember` | an object and a key | the member | Throws when the key is absent. |
| `parseContentRoot` | the file text | the JSON document | The single place a syntax error is reported with its line and column. |

The catalogues and [`level_data.cpp`](../app/content/level_data.cpp) build on those with a
second set of verbs.

| Verb | Example | Meaning |
| --- | --- | --- |
| `parse...` | `parseItemCatalog(text, sourceName)` | Text to typed data. Never opens a file. |
| `load...` | `loadItemCatalog(path)` | Reads the file, then calls the matching `parse...`. Every name that touches the filesystem begins with `load`, including `loadContentText`, the primitive the others build on. |
| `validate...` | `validateItemCatalog(catalog)` | Authoring rules applied to typed data. |
| `compose...` | `composeActor(definition, ...)` | Authoring data plus runtime context, such as a texture ID and a spawn position, to a runtime value. The family has no fixed parameter list: `composeItemStack(catalog, stack)` resolves an authoring name to an `ItemId` and takes neither. It continues into `app/game`, where `composeGameLevel` and `composePlayer` assemble a whole level from the same catalogues. |
| a noun | `itemDefinition(catalog, name)` | A lookup. Returns the entry, or throws when the name is unknown. `animationSet` and `levelPath` read the same way. |

`parse...` never opens a file, so every loader can be tested with a string literal instead of
a fixture. `validate...` is separate from `parse...`, so the same rules apply whether content
arrived as JSON or was written in C++; the loader contributes the filename and the validator
supplies the rest of the path.

The file-local helpers use the same vocabulary. `jsonActorPlacement` and `jsonExitPlacement`
in `level_data.cpp` convert a value, `readFeetPosition` finds one of two spellings by key,
and `checkLegendSymbols` asserts and returns nothing. `level_data.cpp` adds one more verb:

| Verb | Example | Meaning |
| --- | --- | --- |
| `expand...` | `expandObjectLegend(root, ...)` | A JSON document to a rewritten JSON document. It turns the map-symbol shorthand into explicit placements so both authoring forms reach the parse in the same shape. |
