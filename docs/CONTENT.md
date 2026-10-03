# Content and Level Format

The authoring reference for the files under `assets`. Levels place named definitions;
definitions configure engine components and select enum tactics for NPCs.
Behaviour, movement, combat and pathfinding stay in C++.

- Unknown fields are rejected, so misspellings are reported.
- Errors name the file and either a field path (`items.herb.maximumStack`, `map[2][7]`)
  or, for a JSON syntax error, a line and column.
- Every shared definition is validated, even when no level uses it.
- Shared catalogs load once at startup; restart the game to reload them.
  A level file loads when the level starts.
- Units are pixels, seconds and pixels per second. Sprite regions are atlas pixels.

[ARCHITECTURE.md](ARCHITECTURE.md#data-driven-level-boundary) explains how the loaders
turn these files into the game.

## Files

| File                                                             | Holds                                                 | Loader                                                                                                                 |
| ---------------------------------------------------------------- | ----------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------- |
| [`levels/levels.json`](../assets/levels/levels.json)             | Start level, camera dead zone, level numbers to files | [`level_catalog.cpp`](../app/content/level_catalog.cpp)                                                                |
| `levels/level_N.json`                                            | A level's map, legends and placements                 | [`level_data.cpp`](../app/content/level_data.cpp)                                                                      |
| [`catalogs/tiles.json`](../assets/catalogs/tiles.json)           | Tile size and tiles                                   | [`tile_catalog.cpp`](../app/content/tile_catalog.cpp)                                                                  |
| [`catalogs/actors.json`](../assets/catalogs/actors.json)         | The player and every actor definition                 | [`actor_catalog.cpp`](../app/content/actor_catalog.cpp), [`actor_definition.cpp`](../app/content/actor_definition.cpp) |
| [`catalogs/animations.json`](../assets/catalogs/animations.json) | Animation sets                                        | [`animation_catalog.cpp`](../app/content/animation_catalog.cpp)                                                        |
| [`catalogs/items.json`](../assets/catalogs/items.json)           | Inventory items                                       | [`item_catalog.cpp`](../app/content/item_catalog.cpp)                                                                  |
| [`catalogs/pickups.json`](../assets/catalogs/pickups.json)       | World pickups                                         | [`pickup_catalog.cpp`](../app/content/pickup_catalog.cpp)                                                              |
| [`catalogs/exits.json`](../assets/catalogs/exits.json)           | Exit bodies and sprites                               | [`exit_catalog.cpp`](../app/content/exit_catalog.cpp)                                                                  |
| [`catalogs/hud.json`](../assets/catalogs/hud.json)               | HUD icon regions                                      | [`hud_catalog.cpp`](../app/content/hud_catalog.cpp)                                                                    |

Every catalog is required, even when empty. Every sprite region, frame and icon must lie inside the atlas.

Atlas dimensions and artwork layout come from the asset files. Catalog regions
must match that layout; repacking an atlas requires updating the affected regions.
Collision body sizes and animation timings are configured independently of the art.

Dimensions and coordinates in the examples below are illustrative, not required
asset sizes. Choose them for your content within the constraints of each field.

## Level catalog

```json
{
  "startLevel": 1,
  "cameraDeadZone": [80, 45],
  "levels": [{ "number": 1, "file": "level_1.json" }]
}
```

| Field            | Meaning                                                                                |
| ---------------- | -------------------------------------------------------------------------------------- |
| `startLevel`     | The `number` of the first level.                                                       |
| `cameraDeadZone` | The part of the 320 by 180 view the player moves in before the camera follows.         |
| `levels`         | `number`, a positive unique ID that exits refer to, and `file`, relative to this file. |

Levels can be renamed, added or removed by editing the catalog, without changing C++.

## Level files

```json
{
  "tileLegend": { ".": "empty", "#": "stone" },
  "objectLegend": { "Z": { "type": "actor", "definition": "zombie" } },
  "map": ["..Z.....", "########"],
  "playerSpawnCell": [1, 0],
  "pickups": [{ "definition": "medicine_box", "spawnCell": [4, 0] }],
  "exit": { "definition": "bunker_door", "spawnCell": [6, 0], "nextLevel": 2 }
}
```

| Field                                  | Required | Meaning                                                                            |
| -------------------------------------- | -------- | ---------------------------------------------------------------------------------- |
| `tileLegend`                           | Yes      | One-character map symbols to tile names in `tiles.json`.                           |
| `objectLegend`                         | No       | One-character map symbols that place objects. See [Object legend](#object-legend). |
| `map`                                  | Yes      | Rows of equal, nonzero length. Every symbol is in one legend.                      |
| `playerSpawnCell` or `playerSpawnFeet` | Yes\*    | Where the player starts.                                                           |
| `actors`                               | No       | Actor placements.                                                                  |
| `pickups`                              | No       | Pickup placements.                                                                 |
| `exit`                                 | Yes\*    | The exit placement. Without `nextLevel`, it completes the game.                    |

\* Or a marker in `objectLegend`. A level has exactly one player and one exit.

### Positions

Coordinates start at the top-left, with Y pointing down. A cell is `[column, row]`.
Each position is given one way: `…Cell` puts an object's feet at the bottom centre of
that cell, and `…Feet` gives that point in world pixels. Feet are a reference point; an
object placed by them need not stand on the ground.

### Placements

| Placement | Fields                                                                                                                                                          |
| --------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Actor     | `definition` from `actors.json`; `spawnCell` or `spawnFeet`; optional `patrol` with `firstCell`/`firstFeet` and `secondCell`/`secondFeet`, absolute positions.  |
| Pickup    | `spawnCell` or `spawnFeet`, and either `definition` from `pickups.json` or an inline `item`, positive `quantity` and `bodySize`.                                |
| Exit      | `definition` from `exits.json`; `spawnCell` or `spawnFeet`; optional `requirement` (`item`, positive `quantity`), `consumeItem` (default `false`), `nextLevel`. |

Bodies come from definitions, never placements. An inline pickup draws its item's icon.
A pickup falls until it rests on a tile, and falls again if that tile breaks.

### Object legend

An entry has a `type`, `player`, `actor`, `pickup` or `exit`, and the same fields as
that placement except its position, which each marked cell supplies. A player entry has
no other fields.

```json
"objectLegend": {
  "P": { "type": "player" },
  "K": { "type": "pickup", "definition": "key" },
  "E": { "type": "exit", "definition": "bunker_door", "requirement": { "item": "key", "quantity": 1 } }
}
```

- A marked cell becomes empty terrain.
- A symbol cannot be in both legends.
- Explicit placements come first, then markers in row order, left to right.
- Every marker of a symbol shares its entry. Use an explicit placement for an object
  with its own patrol, on non-empty terrain, or off a cell's centre.

## Tiles

`tiles.json` has `tileSize`, the side of a tile in world pixels, and `tiles`, by name.

| Field            | Required | Meaning                                                                                |
| ---------------- | -------- | -------------------------------------------------------------------------------------- |
| `blocksMovement` | Yes      | Solid to bodies.                                                                       |
| `blocksSight`    | Yes      | Stops NPC sight.                                                                       |
| `sprite`         | \*       | `{ "position": [x, y] }`; the region is one tile square. `empty` has none.             |
| `climbable`      | No       | A climber can grip its walls and underside. Map edges never are.                       |
| `breaksInto`     | No       | The tile it becomes when a `breaksTiles` shot hits it. Chain tiles to break in stages. |

\* Required on every tile but `empty`, which must allow movement and sight.

## Actors

```json
{
  "player": "hero",
  "actors": {
    "hero": {
      "bodySize": [12, 20],
      "team": "player",
      "health": 3,
      "inventorySlots": 6,
      "animations": "player",
      "platformer": {}
    },
    "bat": {
      "bodySize": [10, 8],
      "team": "enemy",
      "health": 1,
      "animations": "bat",
      "spriteAnchor": "center",
      "flying": { "speed": 40 },
      "senses": { "noticeDistance": 60 },
      "bite": {}
    }
  }
}
```

`player` names the player's definition, which needs `health` and `inventorySlots` and
no `senses`.

| Field            | Meaning                                                                     |
| ---------------- | --------------------------------------------------------------------------- |
| `bodySize`       | Required. The body's size.                                                  |
| `team`           | `player`, `enemy` or `neutral` (default). Attacks need a non-neutral team.  |
| `facing`         | `left` or `right` (default).                                                |
| `animations`     | A set in `animations.json`.                                                 |
| `spriteAnchor`   | `feet` (default) or `center`.                                               |
| `health`         | Positive.                                                                   |
| `inventorySlots` | Positive.                                                                   |
| `platformer`     | Walking and jumping. Exactly one of `platformer` and `flying`.              |
| `flying`         | Flying.                                                                     |
| `surfaceClimb`   | Climbing walls and ceilings. Needs `platformer`.                            |
| `senses`         | Makes the actor an NPC.                                                     |
| `tactic`         | `pursuer` (default), `keepDistance`, `coward` or `charger`. Needs `senses`. |
| `bite`           | A melee attack. At most one of `bite` and `ranged`.                         |
| `ranged`         | A projectile attack.                                                        |
| `contactDamage`  | Damage on touch, when a script asks for it. Works with either attack.       |

`bodySize` defines the collision body's width and height independently of sprite
frame dimensions. Neither the body nor the frames need to be square or match the
tile size. Changing animation poses does not change the body's dimensions. The
debug overlay (F1) outlines the body in red and the art in grey.

A component object may leave out any field to keep its default, so `{}` is all defaults.

| Component       | Fields (default)                                                                                                                                                                                                                   |
| --------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `platformer`    | `maximumSpeed` (100), `groundAcceleration` (800), `airAcceleration` (400), `groundDeceleration` (1000), `jumpSpeed` (240), `gravity`, `jumpReleaseGravity`, `maximumFallSpeed`, `coyoteDuration` (0.1), `jumpBufferDuration` (0.1) |
| `flying`        | `speed` (60)                                                                                                                                                                                                                       |
| `surfaceClimb`  | `speed` (60)                                                                                                                                                                                                                       |
| `senses`        | `noticeDistance` (96), `targetMemoryDuration` (1.5), `searchDuration` (2), `standoffDistance` (48)                                                                                                                                 |
| `bite`          | `damage` (1), `hitboxSize` ([10, 8]), `reach` (4), `windupDuration` (0.12), `activeDuration` (0.08), `recoveryDuration` (0.3)                                                                                                      |
| `ranged`        | `damage` (1), `projectileSize` ([4, 2]), `projectileSpeed` (180), `projectileLifetime` (2), `shootDuration` (0.15), `recoveryDuration` (0.2), `breaksTiles` (false), `sprite`                                                      |
| `contactDamage` | `damage` (1)                                                                                                                                                                                                                       |

A `sprite`, here and for items, pickups and exits, has `position` and `size`, the atlas
region, which is also its size in the world; and `anchor`, `feet` (default) or `center`.

## Animation sets

`animations.json` holds sets by name. A set has the clips `idle`, `move`, `jump`, `fall`,
`attack` and `death`, all required.

```json
"move": {
  "frames": [{ "position": [64, 0], "size": [32, 24] }, { "position": [96, 0], "size": [32, 24] }],
  "frameDuration": 0.16,
  "looping": true
}
```

| Field           | Meaning                                                                       |
| --------------- | ----------------------------------------------------------------------------- |
| `frames`        | At least one atlas region, played in order. Every frame in a set is one size. |
| `frameDuration` | Seconds per frame. Positive.                                                  |
| `looping`       | Whether the clip repeats; otherwise it holds its last frame.                  |

How the engine picks a clip is in [Animation](ARCHITECTURE.md#animation).

## Items

```json
"health_potion": {
  "name": "Health potion",
  "icon": { "position": [80, 84], "size": [16, 16] },
  "maximumStack": 5,
  "effect": "heal",
  "effectAmount": 2
}
```

| Field          | Meaning                                      |
| -------------- | -------------------------------------------- |
| `name`         | The label shown in the HUD.                  |
| `icon`         | A [sprite](#actors), drawn in the inventory. |
| `maximumStack` | Positive.                                    |
| `effect`       | `none` (default) or `heal`.                  |
| `effectAmount` | Zero for `none`, positive for `heal`.        |

Saves, if added, should store item names: item IDs are assigned at load and can change.

## Pickups

```json
"medicine_box": { "item": "health_potion", "quantity": 2, "bodySize": [16, 16] }
```

| Field      | Meaning                                                  |
| ---------- | -------------------------------------------------------- |
| `item`     | An item in `items.json`.                                 |
| `quantity` | Positive.                                                |
| `bodySize` | The body the player touches to collect it.               |
| `sprite`   | Optional; without one, the pickup draws its item's icon. |

## Exits

```json
"bunker_door": { "bodySize": [16, 16], "sprite": { "position": [112, 96], "size": [16, 16] } }
```

`bodySize` and `sprite` are required. The requirement, consumption and next level belong
to each [placement](#placements), so doors that look alike can lead to different levels.

## HUD icons

`hud.json` has the regions `fullHeart`, `emptyHeart` and `bag`, each with `position` and
`size`. The HUD draws them all at one size.
