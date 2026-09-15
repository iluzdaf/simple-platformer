#include "example_game.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/actor/lifecycle.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/render_scene.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

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
        static const AnimationClip death{
            AnimationName::Death, {{{1.0F, 0.0F}, {1.0F, 1.0F}}}, 0.4F, false};

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
        case AnimationName::Death:
            return death;
        case AnimationName::Bite:
            throw std::invalid_argument("The Phase 5 player has no bite animation");
        }

        throw std::invalid_argument("Animation name is invalid");
    }

    void updatePlayerAnimation(simple_platformer::Actor& player, float deltaTime)
    {
        using simple_platformer::AnimationName;
        using simple_platformer::LifeState;

        if (!player.platformerMovement.has_value() || !player.sprite.has_value() ||
            !player.animator.has_value())
        {
            throw std::logic_error("The example player is missing a required component");
        }

        const AnimationName selected =
            player.life == LifeState::Dying
                ? AnimationName::Death
                : simple_platformer::selectMovementAnimation(
                      player.platformerMovement->grounded, player.body.velocity);
        simple_platformer::updateAnimation(
            *player.animator, *player.sprite, selected, clipFor(selected), deltaTime);
    }

    simple_platformer::Actor makePlayer(int textureId)
    {
        simple_platformer::Actor player;
        player.body = {{{32.0F, 196.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
        player.platformerMovement = simple_platformer::PlatformerMovement{};
        player.platformerMovement->grounded = true;
        player.sprite = simple_platformer::Sprite{
            textureId, {{1.0F, 0.0F}, {1.0F, 1.0F}}, player.body.bounds.size};
        player.animator = simple_platformer::Animator{};
        player.health = simple_platformer::Health{3, 3};
        return player;
    }
}

namespace simple_platformer
{
    ExampleGame::ExampleGame(int textureId) : map(makeLevel())
    {
        const ActorId player = world.addActor(makePlayer(textureId));
        world.setPlayer(player, {38.0F, 208.0F});
    }

    void ExampleGame::update(const InputIntentions& intentions, float deltaTime)
    {
        Actor* player = world.findActor(world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The example game has no player");
        }

        player->intentions = intentions;
        updateActorMovement(map, world, deltaTime);
        updateLifeState(world, requests, deltaTime);

        player = world.findActor(world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The example game has no player after lifecycle update");
        }
        updatePlayerAnimation(*player, deltaTime);
    }

    RenderScene ExampleGame::buildScene() const
    {
        const Actor* player = world.findActor(world.playerId());
        if (player == nullptr || !player->sprite.has_value())
        {
            throw std::logic_error("The example player is missing its sprite");
        }

        const Camera camera = makeLockedCamera(map, player->body.bounds);
        return buildRenderScene(map, player->sprite.value().textureId, camera, world);
    }
}
