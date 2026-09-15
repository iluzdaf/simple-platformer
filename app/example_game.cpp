#include "example_game.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/actor/actor_system.hpp"
#include "simple_platformer/actor/lifecycle.hpp"
#include "simple_platformer/combat/attack_system.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/combat/projectile_system.hpp"
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
        static const AnimationClip bite{
            AnimationName::Bite, {{{3.0F, 0.0F}, {1.0F, 1.0F}}}, 0.1F, true};

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
            return bite;
        }

        throw std::invalid_argument("Animation name is invalid");
    }

    void updateActorAnimations(simple_platformer::World& world, float deltaTime)
    {
        using simple_platformer::AnimationName;
        using simple_platformer::BitePhase;
        using simple_platformer::LifeState;

        for (simple_platformer::Actor& actor : world.actors())
        {
            if (!actor.animator.has_value())
            {
                continue;
            }
            if (!actor.platformerMovement.has_value() || !actor.sprite.has_value())
            {
                throw std::logic_error("An animated actor is missing a required component");
            }

            const bool biting = actor.bite.has_value() && actor.bite->phase != BitePhase::Ready;
            const AnimationName selected = simple_platformer::selectActorAnimation(
                actor.life == LifeState::Dying,
                biting,
                actor.platformerMovement->grounded,
                actor.body.velocity);
            simple_platformer::updateAnimation(
                *actor.animator, *actor.sprite, selected, clipFor(selected), deltaTime);
        }
    }

    void requestNpcAttacks(simple_platformer::World& world)
    {
        for (simple_platformer::Actor& actor : world.actors())
        {
            if (actor.team == simple_platformer::Team::Enemy && actor.bite.has_value())
            {
                actor.intentions.primaryAttackPressed = true;
            }
        }
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
        player.team = simple_platformer::Team::Player;
        simple_platformer::RangedWeapon weapon;
        weapon.projectileSprite = {textureId, {{4.0F, 0.0F}, {1.0F, 1.0F}}, weapon.projectileSize};
        player.rangedWeapon = weapon;
        return player;
    }

    simple_platformer::Actor makeBitingNpc(int textureId)
    {
        simple_platformer::Actor npc;
        npc.body = {{{176.0F, 196.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
        npc.platformerMovement = simple_platformer::PlatformerMovement{};
        npc.platformerMovement->grounded = true;
        npc.facing = simple_platformer::Facing::Left;
        npc.sprite = simple_platformer::Sprite{
            textureId, {{2.0F, 0.0F}, {1.0F, 1.0F}}, npc.body.bounds.size};
        npc.animator = simple_platformer::Animator{};
        npc.health = simple_platformer::Health{3, 3};
        npc.team = simple_platformer::Team::Enemy;
        npc.bite = simple_platformer::BiteAttack{};
        return npc;
    }
}

namespace simple_platformer
{
    ExampleGame::ExampleGame(int textureId) : map(makeLevel())
    {
        const ActorId player = world.addActor(makePlayer(textureId));
        world.setPlayer(player, {38.0F, 208.0F});
        world.addActor(makeBitingNpc(textureId));
    }

    void ExampleGame::update(const InputIntentions& intentions, float deltaTime)
    {
        Actor* player = world.findActor(world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The example game has no player");
        }

        player->intentions = intentions;
        requestNpcAttacks(world);
        updateActorMovement(map, world, deltaTime);
        updateAttacks(world, requests, deltaTime);
        updateProjectiles(map, world, requests, deltaTime);
        updateLifeState(world, requests, deltaTime);

        player = world.findActor(world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The example game has no player after lifecycle update");
        }
        updateActorAnimations(world, deltaTime);
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
