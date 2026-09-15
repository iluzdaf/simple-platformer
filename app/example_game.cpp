#include "example_game.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/render_scene.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/tile_map.hpp"

namespace
{
    simple_platformer::TileMap makeLevel()
    {
        constexpr int Width = 60;
        constexpr int Height = 14;
        std::vector<int> tiles(static_cast<std::size_t>(Width * Height), 0);

        const auto fill = [&tiles](int row, int firstColumn, int lastColumn)
        {
            for (int column = firstColumn; column <= lastColumn; ++column)
            {
                const std::size_t index =
                    static_cast<std::size_t>(row) * static_cast<std::size_t>(Width) +
                    static_cast<std::size_t>(column);
                tiles[index] = 1;
            }
        };

        fill(Height - 1, 0, Width - 1);
        fill(10, 6, 12);
        fill(8, 18, 24);
        fill(11, 30, 38);
        fill(8, 42, 48);
        fill(11, 53, 57);

        return {Width, Height, std::move(tiles), {{false}, {true, {{0.0F, 0.0F}, {1.0F, 1.0F}}}}};
    }

    const simple_platformer::AnimationClip& clipFor(simple_platformer::AnimationName name)
    {
        using simple_platformer::AnimationClip;
        using simple_platformer::AnimationName;

        static const AnimationClip idle{
            AnimationName::Idle, {{{1.0F, 0.0F}, {1.0F, 1.0F}}}, 0.15F, true};
        static const AnimationClip run{
            AnimationName::Run,
            {{{2.0F, 0.0F}, {1.0F, 1.0F}}, {{3.0F, 0.0F}, {1.0F, 1.0F}}},
            0.12F,
            true};
        static const AnimationClip jump{
            AnimationName::Jump, {{{4.0F, 0.0F}, {1.0F, 1.0F}}}, 0.15F, true};
        static const AnimationClip fall{
            AnimationName::Fall, {{{5.0F, 0.0F}, {1.0F, 1.0F}}}, 0.15F, true};

        switch (name)
        {
        case AnimationName::Idle:
            return idle;
        case AnimationName::Run:
            return run;
        case AnimationName::Jump:
            return jump;
        case AnimationName::Fall:
            return fall;
        case AnimationName::Bite:
        case AnimationName::Death:
            throw std::invalid_argument("The Phase 4 player has no combat animation");
        }

        throw std::invalid_argument("Animation name is invalid");
    }
}

namespace simple_platformer
{
    ExampleGame::ExampleGame()
        : map(makeLevel()), player{{{32.0F, 196.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}}
    {
        movement.grounded = true;
    }

    void ExampleGame::update(const InputIntentions& intentions, float deltaTime)
    {
        updatePlatformerMovement(map, player, movement, intentions, facing, deltaTime);

        const AnimationName selected = selectMovementAnimation(movement.grounded, player.velocity);
        if (selected != animation)
        {
            animation = selected;
            animationElapsed = 0.0F;
        }
        else
        {
            animationElapsed += deltaTime;
        }
    }

    RenderScene ExampleGame::buildScene(int textureId) const
    {
        const Camera camera = makeLockedCamera(map, player.bounds);
        const Sprite playerSprite{
            textureId, frameAt(clipFor(animation), animationElapsed), player.bounds.size};
        return buildRenderScene(map, textureId, camera, playerSprite, player.bounds, facing);
    }
}
