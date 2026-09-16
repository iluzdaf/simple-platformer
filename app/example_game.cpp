#include "example_game.hpp"

#include "actor_debug.hpp"
#include "example_animations.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/animation_system.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/render_scene.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_simulation.hpp"

namespace
{
    simple_platformer::Animator makeAnimator(simple_platformer::AnimationSet animations)
    {
        simple_platformer::Animator animator;
        animator.animationSet = std::move(animations);
        return animator;
    }

    simple_platformer::Sprite makeActorSprite(
        int textureId,
        const simple_platformer::AnimationSet& animations)
    {
        const simple_platformer::AnimationClip& idle =
            simple_platformer::clipFor(animations, simple_platformer::AnimationName::Idle);
        return {textureId, idle.frames.front(), idle.frames.front().size};
    }

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

        return {
            Width, Height, std::move(tiles), {{false}, {true, {{0.0F, 192.0F}, {16.0F, 16.0F}}}}};
    }

    simple_platformer::RangedWeapon makeRangedWeapon(int textureId, simple_platformer::Team team)
    {
        simple_platformer::RangedWeapon weapon;
        const simple_platformer::SpriteRegion region =
            team == simple_platformer::Team::Player
                ? simple_platformer::SpriteRegion{{32.0F, 192.0F}, {8.0F, 4.0F}}
                : simple_platformer::SpriteRegion{{64.0F, 192.0F}, {8.0F, 4.0F}};
        weapon.projectileSprite = {textureId, region, region.size};
        return weapon;
    }

    simple_platformer::Actor makePlayer(int textureId)
    {
        simple_platformer::AnimationSet animations = simple_platformer::makePlayerAnimations();

        simple_platformer::Actor player;
        player.body = {{{32.0F, 188.0F}, {12.0F, 20.0F}}, {0.0F, 0.0F}};
        player.platformerMovement = simple_platformer::PlatformerMovement{};
        player.platformerMovement->grounded = true;
        player.sprite = makeActorSprite(textureId, animations);
        player.animator = makeAnimator(std::move(animations));
        player.health = simple_platformer::Health{3, 3};
        player.team = simple_platformer::Team::Player;
        player.rangedWeapon = makeRangedWeapon(textureId, player.team);
        return player;
    }

    simple_platformer::Actor makeNpc(
        int textureId,
        glm::vec2 spawnFeet,
        glm::vec2 bodySize,
        simple_platformer::SpriteAnchor spriteAnchor,
        simple_platformer::Patrol patrol,
        simple_platformer::AnimationSet animations)
    {
        simple_platformer::Actor npc;
        npc.body.bounds.size = bodySize;
        simple_platformer::placeFeetAt(npc.body.bounds, spawnFeet);
        npc.facing = simple_platformer::Facing::Left;
        npc.sprite = makeActorSprite(textureId, animations);
        npc.sprite->anchor = spriteAnchor;
        npc.animator = makeAnimator(std::move(animations));
        npc.health = simple_platformer::Health{3, 3};
        npc.team = simple_platformer::Team::Enemy;
        npc.brain = simple_platformer::NpcBrain{};
        npc.senses = simple_platformer::NpcSenses{};
        npc.patrol = patrol;
        npc.pathFollower = simple_platformer::PathFollower{};
        return npc;
    }

    simple_platformer::Actor makeZombie(
        int textureId,
        glm::vec2 spawnFeet,
        simple_platformer::Patrol patrol)
    {
        simple_platformer::Actor npc = makeNpc(
            textureId,
            spawnFeet,
            {12.0F, 20.0F},
            simple_platformer::SpriteAnchor::BodyFeet,
            patrol,
            simple_platformer::makeZombieAnimations());
        npc.platformerMovement = simple_platformer::PlatformerMovement{};
        npc.platformerMovement->grounded = true;
        npc.bite = simple_platformer::BiteAttack{};
        return npc;
    }

    simple_platformer::Actor makeBat(
        int textureId,
        glm::vec2 spawnFeet,
        simple_platformer::Patrol patrol)
    {
        simple_platformer::Actor npc = makeNpc(
            textureId,
            spawnFeet,
            {12.0F, 8.0F},
            simple_platformer::SpriteAnchor::BodyCenter,
            patrol,
            simple_platformer::makeBatAnimations());
        npc.flyingMovement = simple_platformer::FlyingMovement{};
        npc.bite = simple_platformer::BiteAttack{};
        return npc;
    }

    simple_platformer::Actor makeZombieSoldier(
        int textureId,
        glm::vec2 spawnFeet,
        simple_platformer::Patrol patrol)
    {
        simple_platformer::Actor npc = makeNpc(
            textureId,
            spawnFeet,
            {12.0F, 20.0F},
            simple_platformer::SpriteAnchor::BodyFeet,
            patrol,
            simple_platformer::makeZombieSoldierAnimations());
        npc.platformerMovement = simple_platformer::PlatformerMovement{};
        npc.platformerMovement->grounded = true;
        npc.rangedWeapon = makeRangedWeapon(textureId, npc.team);
        return npc;
    }
}

namespace simple_platformer
{
    ExampleGame::ExampleGame(int textureId) : map(makeLevel())
    {
        const ActorId player = world.addActor(makePlayer(textureId));
        world.setPlayer(player, {38.0F, 208.0F});
        world.addActor(
            makeZombie(textureId, {456.0F, 208.0F}, {{456.0F, 208.0F}, {488.0F, 176.0F}, true}));
        world.addActor(
            makeBat(textureId, {176.0F, 144.0F}, {{176.0F, 144.0F}, {248.0F, 96.0F}, true}));
        world.addActor(makeZombieSoldier(
            textureId, {286.0F, 208.0F}, {{286.0F, 208.0F}, {350.0F, 208.0F}, true}));

        const Actor* playerActor = world.findActor(player);
        if (playerActor == nullptr)
        {
            throw std::logic_error("The example game could not initialise its camera");
        }
        cameraController = makeCameraController(map, playerActor->body.bounds, {80.0F, 45.0F});
    }

    void ExampleGame::update(const InputIntentions& intentions, float deltaTime)
    {
        Actor* player = world.findActor(world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The example game has no player");
        }

        player->intentions = intentions;
        updateWorldSimulation(map, world, deltaTime);

        player = world.findActor(world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The example game has no player after lifecycle update");
        }
        followTarget(cameraControllerValue(), map, player->body.bounds);
        simple_platformer::updateActorAnimations(world, deltaTime);
    }

    glm::vec2 ExampleGame::playerAimDirection(glm::vec2 screenPosition) const
    {
        const Actor* player = world.findActor(world.playerId());
        if (player == nullptr)
        {
            throw std::logic_error("The example game has no player");
        }

        return screenToWorld(currentCamera(), screenPosition) - centerOf(player->body.bounds);
    }

    RenderScene ExampleGame::buildScene() const
    {
        const Actor* player = world.findActor(world.playerId());
        if (player == nullptr || !player->sprite.has_value())
        {
            throw std::logic_error("The example player is missing its sprite");
        }

        return buildRenderScene(map, player->sprite.value().textureId, currentCamera(), world);
    }

    ActorDebugScene ExampleGame::actorDebugScene() const
    {
        constexpr float AtlasWidth = 160.0F;
        return makeActorDebugScene(world, map, cameraControllerValue(), AtlasWidth);
    }

    Camera ExampleGame::currentCamera() const
    {
        return cameraControllerValue().camera;
    }

    CameraController& ExampleGame::cameraControllerValue()
    {
        if (!cameraController.has_value())
        {
            throw std::logic_error("The example game camera is not initialised");
        }
        return *cameraController;
    }

    const CameraController& ExampleGame::cameraControllerValue() const
    {
        if (!cameraController.has_value())
        {
            throw std::logic_error("The example game camera is not initialised");
        }
        return *cameraController;
    }
}
