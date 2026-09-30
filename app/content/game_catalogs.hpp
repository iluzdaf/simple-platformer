#pragma once

#include <filesystem>
#include <glm/vec2.hpp>
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
        HudIcons hudIcons;
    };

    // Loads every catalog in the directory, then checks their regions fit in an atlas of
    // this size, in pixels.
    GameCatalogs loadGameCatalogs(
        const std::filesystem::path& catalogDirectory,
        glm::ivec2 atlasSize);

    // Rejects the first region in any catalog that runs past the atlas, naming its file in
    // the directory and its field.
    void validateAtlasRegions(
        const GameCatalogs& catalogs,
        glm::ivec2 atlasSize,
        const std::filesystem::path& catalogDirectory);
}
