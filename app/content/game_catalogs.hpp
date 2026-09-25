#pragma once

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
    // Shared definitions, not live actors or levels. Keep these for the game session.
    struct GameCatalogs
    {
        TileCatalog tiles;
        AnimationCatalog animations;
        MachineCatalog machines;
        ActorCatalog actors;
        ItemCatalog items;
        PickupCatalog pickups;
        ExitCatalog exits;
    };

    GameCatalogs loadGameCatalogs(const std::filesystem::path& levelDirectory);
}
