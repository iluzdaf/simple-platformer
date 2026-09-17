#include "example_content.hpp"

#include "example_animations.hpp"
#include "example_items.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/level_exit.hpp"
#include "simple_platformer/world/pickup.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"

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

    simple_platformer::TileMap makeLevelOneMap()
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

    void populateLevelOne(simple_platformer::World& world, int textureId)
    {
        world.addActor(
            makeZombie(textureId, {456.0F, 208.0F}, {{456.0F, 208.0F}, {488.0F, 176.0F}, true}));
        world.addActor(
            makeBat(textureId, {176.0F, 144.0F}, {{176.0F, 144.0F}, {248.0F, 96.0F}, true}));
        world.addActor(makeZombieSoldier(
            textureId, {286.0F, 208.0F}, {{286.0F, 208.0F}, {350.0F, 208.0F}, true}));

        world.addPickup({{{80.0F, 196.0F}, {12.0F, 12.0F}}, {simple_platformer::Coin, 5}});
        world.addPickup({{{128.0F, 192.0F}, {16.0F, 16.0F}}, {simple_platformer::HealthPotion, 2}});
        world.addPickup({{{220.0F, 196.0F}, {12.0F, 12.0F}}, {simple_platformer::Key, 1}});

        simple_platformer::LevelExit exit;
        exit.bounds = {{936.0F, 176.0F}, {16.0F, 32.0F}};
        exit.requirement = simple_platformer::ItemStack{simple_platformer::Key, 1};
        exit.nextLevel = 2;
        exit.sprite =
            simple_platformer::Sprite{textureId, {{48.0F, 216.0F}, {16.0F, 32.0F}}, {16.0F, 32.0F}};
        world.setExit(exit);
    }

    simple_platformer::TileMap makeLevelTwoMap()
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
        fill(11, 10, 16);
        fill(9, 23, 28);
        fill(11, 38, 46);

        return {
            Width, Height, std::move(tiles), {{false}, {true, {{0.0F, 192.0F}, {16.0F, 16.0F}}}}};
    }

    void populateLevelTwo(simple_platformer::World& world, int textureId)
    {
        world.addActor(
            makeBat(textureId, {400.0F, 112.0F}, {{400.0F, 112.0F}, {480.0F, 144.0F}, true}));
        world.addActor(
            makeZombie(textureId, {680.0F, 208.0F}, {{650.0F, 208.0F}, {730.0F, 208.0F}, true}));

        world.addPickup({{{80.0F, 196.0F}, {12.0F, 12.0F}}, {simple_platformer::Coin, 5}});
        world.addPickup({{{128.0F, 192.0F}, {16.0F, 16.0F}}, {simple_platformer::HealthPotion, 2}});

        simple_platformer::LevelExit exit;
        exit.bounds = {{936.0F, 176.0F}, {16.0F, 32.0F}};
        exit.requirement = simple_platformer::ItemStack{simple_platformer::Key, 1};
        exit.sprite =
            simple_platformer::Sprite{textureId, {{48.0F, 216.0F}, {16.0F, 32.0F}}, {16.0F, 32.0F}};
        world.setExit(exit);
    }
}

namespace simple_platformer
{
    TileMap makeExampleLevel(int level)
    {
        switch (level)
        {
        case 1:
            return makeLevelOneMap();
        case 2:
            return makeLevelTwoMap();
        default:
            throw std::invalid_argument("Unknown example level");
        }
    }

    Actor makeExamplePlayer(int textureId)
    {
        AnimationSet animations = makePlayerAnimations();

        Actor player;
        player.body = {{{32.0F, 188.0F}, {12.0F, 20.0F}}, {0.0F, 0.0F}};
        player.platformerMovement = PlatformerMovement{};
        player.platformerMovement->grounded = true;
        player.sprite = makeActorSprite(textureId, animations);
        player.animator = makeAnimator(std::move(animations));
        player.health = Health{3, 3};
        player.inventory = Inventory{6};
        player.team = Team::Player;
        player.rangedWeapon = makeRangedWeapon(textureId, player.team);
        return player;
    }

    void populateExampleLevel(World& world, int level, int textureId)
    {
        switch (level)
        {
        case 1:
            populateLevelOne(world, textureId);
            return;
        case 2:
            populateLevelTwo(world, textureId);
            return;
        default:
            throw std::invalid_argument("Unknown example level");
        }
    }
}
