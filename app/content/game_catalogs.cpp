#include "game_catalogs.hpp"
#include <filesystem>
#include "actor_catalog.hpp"
#include "animation_catalog.hpp"
#include "tile_catalog.hpp"
#include "item_catalog.hpp"
#include "pickup_catalog.hpp"
#include "exit_catalog.hpp"
#include "machine_catalog.hpp"

namespace simple_platformer
{
    GameCatalogs loadGameCatalogs(const std::filesystem::path& levelDirectory)
    {
        GameCatalogs catalogs;
        catalogs.tiles = loadTileCatalog(levelDirectory / "tiles.json");
        catalogs.animations = loadAnimationCatalog(levelDirectory / "animations.json");
        catalogs.machines = loadMachineCatalog(levelDirectory / "machines.json");
        // Actor definitions reference the animation sets and machines loaded above.
        catalogs.actors = loadActorCatalog(
            levelDirectory / "actors.json", catalogs.animations, catalogs.machines);
        catalogs.items = loadItemCatalog(levelDirectory / "items.json");
        // Pickup stacks refer to the item definitions loaded above.
        catalogs.pickups = loadPickupCatalog(levelDirectory / "pickups.json", catalogs.items);
        catalogs.exits = loadExitCatalog(levelDirectory / "exits.json");
        return catalogs;
    }
}
