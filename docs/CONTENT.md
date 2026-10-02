# Content and Level Format

The authoring reference for the files under `assets`. Levels place named definitions;
definitions configure engine components; machines and Lua activities decide what NPCs
do. Movement, combat and pathfinding stay in C++.

- Unknown fields are rejected, so misspellings are reported.
- Errors name the file and either a field path (`items.herb.maximumStack`, `map[2][7]`)
  or, for a JSON syntax error, a line and column.
- Every shared definition is validated, even when no level uses it.
- Shared catalogs and Lua scripts load once at startup; restart the game to reload them.
  A level file loads when the level starts.
- Units are pixels, seconds and pixels per second. Sprite regions are atlas pixels.

[ARCHITECTURE.md](ARCHITECTURE.md#data-driven-level-boundary) explains how the loaders
turn these files into the game.

## Files

| File                                                             | Holds                                                 | Loader                                                                                                                       |
| ---------------------------------------------------------------- | ----------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| [`levels/levels.json`](../assets/levels/levels.json)             | Start level, camera dead zone, level numbers to files | [`level_catalog.cpp`](../app/content/level_catalog.cpp)                                                                      |
| `levels/level_N.json`                                            | A level's map, legends and placements                 | [`level_data.cpp`](../app/content/level_data.cpp)                                                                            |
| [`catalogs/tiles.json`](../assets/catalogs/tiles.json)           | Tile size and tiles                                   | [`tile_catalog.cpp`](../app/content/tile_catalog.cpp)                                                                        |
| [`catalogs/actors.json`](../assets/catalogs/actors.json)         | The player and every actor definition                 | [`actor_catalog.cpp`](../app/content/actor_catalog.cpp), [`actor_definition.cpp`](../app/content/actor_definition.cpp)       |
| [`catalogs/animations.json`](../assets/catalogs/animations.json) | Animation sets                                        | [`animation_catalog.cpp`](../app/content/animation_catalog.cpp)                                                              |
| [`catalogs/machines.json`](../assets/catalogs/machines.json)     | NPC state machines                                    | [`machine_catalog.cpp`](../app/content/machine_catalog.cpp)                                                                  |
| [`scripts/*.lua`](../assets/scripts)                             | Lua activities                                        | [`npc_script_catalog.cpp`](../app/content/npc_script_catalog.cpp), [`lua_npc_scripts.cpp`](../scripting/lua_npc_scripts.cpp) |
| [`catalogs/items.json`](../assets/catalogs/items.json)           | Inventory items                                       | [`item_catalog.cpp`](../app/content/item_catalog.cpp)                                                                        |
| [`catalogs/pickups.json`](../assets/catalogs/pickups.json)       | World pickups                                         | [`pickup_catalog.cpp`](../app/content/pickup_catalog.cpp)                                                                    |
| [`catalogs/exits.json`](../assets/catalogs/exits.json)           | Exit bodies and sprites                               | [`exit_catalog.cpp`](../app/content/exit_catalog.cpp)                                                                        |
| [`catalogs/hud.json`](../assets/catalogs/hud.json)               | HUD icon regions                                      | [`hud_catalog.cpp`](../app/content/hud_catalog.cpp)                                                                          |

Every catalog is required, even when empty. A Lua script loads only when a machine
names it. Every sprite region, frame and icon must lie inside the atlas.

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

`tiles.json` has `tileSize`, the side of a tile in world pixels (16), and `tiles`, by name.

| Field            | Required | Meaning                                                                                |
| ---------------- | -------- | -------------------------------------------------------------------------------------- |
| `blocksMovement` | Yes      | Solid to bodies.                                                                       |
| `blocksSight`    | Yes      | Stops NPC sight.                                                                       |
| `sprite`         | \*       | `{ "position": [x, y] }`; the region is one tile square. `empty` has none.             |
| `climbable`      | No       | A climber can grip its walls and underside. Map edges never are.                       |
| `breaksInto`     | No       | The tile it becomes when a `breaksTiles` shot hits it. Chain tiles to break in stages. |

\* Required on every tile but `empty`, which must allow movement and sight. Speeds and
jump heights are tuned for 16-pixel tiles.

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
      "bodySize": [12, 8],
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

| Field            | Meaning                                                                    |
| ---------------- | -------------------------------------------------------------------------- |
| `bodySize`       | Required. The body's size.                                                 |
| `team`           | `player`, `enemy` or `neutral` (default). Attacks need a non-neutral team. |
| `facing`         | `left` or `right` (default).                                               |
| `animations`     | A set in `animations.json`.                                                |
| `spriteAnchor`   | `feet` (default) or `center`.                                              |
| `health`         | Positive.                                                                  |
| `inventorySlots` | Positive.                                                                  |
| `platformer`     | Walking and jumping. Exactly one of `platformer` and `flying`.             |
| `flying`         | Flying.                                                                    |
| `surfaceClimb`   | Climbing walls and ceilings. Needs `platformer`.                           |
| `senses`         | Makes the actor an NPC.                                                    |
| `tactic`         | `pursuer` (default) or `keepDistance`. Needs `senses`.                     |
| `machine`        | A machine in `machines.json`, run instead of the tactic. Needs `senses`.   |
| `bite`           | A melee attack. At most one of `bite` and `ranged`.                        |
| `ranged`         | A projectile attack.                                                       |
| `contactDamage`  | Damage on touch, when a script asks for it. Works with either attack.      |

`bodySize` and the art are independent. The player's frames are 32 by 24 around a body
of 12 by 20, so the gun and a jump do not change how it collides. The debug overlay (F1,
then 2) outlines the body in red and the art in grey.

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

## State machines

`machines.json` holds machines by name. A machine's first state is the one an NPC starts in.

```json
"guard": {
  "states": [
    { "name": "patrol", "does": "patrol" },
    { "name": "flee", "does": { "kind": "lua", "script": "rat", "activity": "flee" } }
  ],
  "transitions": [
    { "from": "patrol", "to": "flee", "when": { "targetKnown": true } },
    { "from": "flee", "to": "patrol", "when": { "targetKnown": false }, "after": 0.5 }
  ]
}
```

| Field   | Meaning                                                                               |
| ------- | ------------------------------------------------------------------------------------- |
| `name`  | The state's name, unique in the machine.                                              |
| `does`  | A built-in activity by name, or a [Lua activity](#lua-activities).                    |
| `from`  | A state, or a list of states for one transition from each.                            |
| `to`    | The state to enter.                                                                   |
| `when`  | [Facts](#facts) and the value each must have. Empty always holds.                     |
| `after` | Optional seconds every condition must hold before the transition fires. Not negative. |

The built-in activities are `idle`, `patrol`, `chase`, `bite`, `shoot`, `search`,
`retreat` and `watch`. A Lua activity is `{ "kind": "lua", "script", "activity" }`, where
`script` is a file in `assets/scripts` without `.lua`.

From one state, the first transition in the list whose conditions have held long enough
fires. Each transition times its own `after`, so two transitions between the same states
do not share elapsed time.

### Facts

| Fact                           | True when                                                             | Notes                                                                 |
| ------------------------------ | --------------------------------------------------------------------- | --------------------------------------------------------------------- |
| `targetKnown`                  | The NPC remembers a living target.                                    | Sight or a heard noise refreshes it; it lasts `targetMemoryDuration`. |
| `targetVisible`                | The NPC sees the target.                                              | Within `noticeDistance` with clear line of sight.                     |
| `targetInBiteRange`            | The visible target overlaps the NPC's bite hitbox.                    | Needs `bite`.                                                         |
| `biteReady`                    | The NPC's bite is ready.                                              | Needs no target.                                                      |
| `targetInSights`               | The target is visible and the NPC has `ranged`.                       | Ignores aim and reload.                                               |
| `targetWithinStandoffDistance` | The remembered target is nearer than `standoffDistance`.              | Measured to its last known feet.                                      |
| `heardLanding`                 | The NPC heard the player land on its ground run.                      | For one update.                                                       |
| `targetOnSameRun`              | The NPC and its remembered target stand on the same continuous floor. | Ignores distance and sight.                                           |
| `targetWithinNoticeDistance`   | The remembered target is within `noticeDistance`.                     | Ignores ground and sight.                                             |
| `movementBlocked`              | The NPC hit a wall, or its ledge guard stopped it.                    | From the last movement update.                                        |
| `hasPatrol`                    | The NPC has a patrol.                                                 |                                                                       |
| `searchTimeUp`                 | The time in this state has reached `searchDuration`.                  | At once when the duration is zero.                                    |

The [boar machine](../assets/catalogs/machines.json) and its
[activities](../assets/scripts/boar.lua) combine `heardLanding`, `targetOnSameRun` and
`targetWithinNoticeDistance` to charge.

### Lua activities

A script returns `{ activities = { name = { enter, update, exit } } }`. `update` is
required, and `enter` and `exit` are optional. Each hook gets `self`, a table kept for
the visit, and a snapshot. `update` also gets the step in seconds, and returns a command
or `nil`. The [Lua boundary](ARCHITECTURE.md#lua-activity-boundary) covers how they run.

| Snapshot        | Meaning                                                                    |
| --------------- | -------------------------------------------------------------------------- |
| `feet`          | The NPC's feet.                                                            |
| `targetFeet`    | The target's known feet, while it is known; otherwise `nil`.               |
| `patrol`        | `firstFeet` and `secondFeet`, when the NPC has a patrol; otherwise `nil`.  |
| `facts`         | The [facts](#facts), and `searches`: whether `searchDuration` is positive. |
| `stateElapsed`  | Seconds in this state.                                                     |
| `routeComplete` | Whether the last route asked for has been followed to its end.             |

| Command                     | Meaning                                                |
| --------------------------- | ------------------------------------------------------ |
| `direction`, `aimDirection` | Movement and aim, as vectors.                          |
| `jumpPressed`, `jumpHeld`   | Jump input.                                            |
| `primaryAttackPressed`      | Bite or shoot.                                         |
| `climbGrip`                 | `"hold"`, `"release"` or `"keep"` (default).           |
| `avoidLedges`               | Stop a walker at a ledge.                              |
| `contactDamage`             | Deal contact damage while touching.                    |
| `routeTo`                   | Follow a route to a point; the engine plans and moves. |
| `aimAt`                     | Aim at a point.                                        |
| `clearRoute`                | Drop the current route.                                |

Positions are `vec2` values, made with `vec2(x, y)`. They have `x` and `y`, `+`, `-`,
negation, `*` and `/` by a number, `==`, `tostring`, and the methods `length()`,
`distance(v)`, `distanceSquared(v)` and `dot(v)`. A command's vectors also accept
`{x, y}` tables. Scripts have the base, math, string and table libraries.

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
  "icon": { "position": [16, 216], "size": [16, 16] },
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
"bunker_door": { "bodySize": [16, 32], "sprite": { "position": [48, 216], "size": [16, 32] } }
```

`bodySize` and `sprite` are required. The requirement, consumption and next level belong
to each [placement](#placements), so doors that look alike can lead to different levels.

## HUD icons

`hud.json` has the regions `fullHeart`, `emptyHeart` and `bag`, each with `position` and
`size`. The HUD draws them all at one size.
