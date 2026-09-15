#pragma once

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/render/render_scene.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace simple_platformer
{
    class ExampleGame
    {
    public:
        ExampleGame();

        void update(const InputIntentions& intentions, float deltaTime);
        RenderScene buildScene(int textureId) const;

    private:
        TileMap map;
        Body player;
        PlatformerMovement movement;
        Facing facing = Facing::Right;
        AnimationName animation = AnimationName::Idle;
        float animationElapsed = 0.0F;
    };
}
