#pragma once

#include "actor_debug.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    struct InputIntentions;
    struct RenderScene;

    class ExampleGame
    {
    public:
        explicit ExampleGame(int textureId);

        void update(const InputIntentions& intentions, float deltaTime);
        RenderScene buildScene() const;
        ActorDebugScene actorDebugScene() const;

    private:
        TileMap map;
        World world;
    };
}
