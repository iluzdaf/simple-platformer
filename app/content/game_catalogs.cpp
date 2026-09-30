#include "game_catalogs.hpp"
#include <filesystem>
#include "actor_catalog.hpp"
#include "animation_catalog.hpp"
#include "tile_catalog.hpp"
#include "item_catalog.hpp"
#include "pickup_catalog.hpp"
#include "exit_catalog.hpp"
#include "hud_catalog.hpp"
#include "machine_catalog.hpp"

namespace simple_platformer
{
    GameCatalogs loadGameCatalogs(const std::filesystem::path& catalogDirectory)
    {
        GameCatalogs catalogs;
        catalogs.tiles = loadTileCatalog(catalogDirectory / "tiles.json");
        catalogs.animations = loadAnimationCatalog(catalogDirectory / "animations.json");
        catalogs.machines = loadMachineCatalog(catalogDirectory / "machines.json");
        // Actor definitions reference the animation sets and machines loaded above.
        catalogs.actors = loadActorCatalog(
            catalogDirectory / "actors.json", catalogs.animations, catalogs.machines);
        catalogs.items = loadItemCatalog(catalogDirectory / "items.json");
        // Pickup stacks refer to the item definitions loaded above.
        catalogs.pickups = loadPickupCatalog(catalogDirectory / "pickups.json", catalogs.items);
        catalogs.exits = loadExitCatalog(catalogDirectory / "exits.json");
        catalogs.hudIcons = loadHudIcons(catalogDirectory / "hud.json");
        return catalogs;
    }
}
