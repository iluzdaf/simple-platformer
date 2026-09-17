#include "example_content.hpp"

#include "example_animations.hpp"
#include "example_items.hpp"
#include "level_catalog.hpp"
#include "example_level_data.hpp"

#include <optional>
#include <stdexcept>
#include <utility>

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
    constexpr float ExamplePickupSideLength = 16.0F;

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
        std::optional<simple_platformer::Patrol> patrol,
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
        std::optional<simple_platformer::Patrol> patrol)
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
        std::optional<simple_platformer::Patrol> patrol)
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
        std::optional<simple_platformer::Patrol> patrol)
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

    simple_platformer::Actor makeActor(
        int textureId,
        const simple_platformer::ExampleActorPlacement& placement)
    {
        switch (placement.type)
        {
        case simple_platformer::ExampleActorType::Zombie:
            return makeZombie(textureId, placement.spawnFeet, placement.patrol);
        case simple_platformer::ExampleActorType::Bat:
            return makeBat(textureId, placement.spawnFeet, placement.patrol);
        case simple_platformer::ExampleActorType::ZombieSoldier:
            return makeZombieSoldier(textureId, placement.spawnFeet, placement.patrol);
        }
        throw std::logic_error("Unknown example actor type");
    }

    simple_platformer::TileMap makeMap(const simple_platformer::ExampleLevelData& data)
    {
        return simple_platformer::TileMap::fromAscii(
            data.mapRows, {{false}, {true, {{0.0F, 192.0F}, {16.0F, 16.0F}}}});
    }

    simple_platformer::Pickup makePickup(const simple_platformer::ExamplePickupPlacement& placement)
    {
        simple_platformer::Pickup pickup;
        pickup.bounds.size = {ExamplePickupSideLength, ExamplePickupSideLength};
        simple_platformer::placeFeetAt(pickup.bounds, placement.spawnFeet);
        pickup.stack = placement.stack;
        return pickup;
    }

    simple_platformer::LevelExit makeExit(
        int textureId,
        const simple_platformer::ExampleExitPlacement& placement)
    {
        simple_platformer::LevelExit exit;
        exit.bounds.size = {16.0F, 32.0F};
        simple_platformer::placeFeetAt(exit.bounds, placement.spawnFeet);
        exit.requirement = placement.requirement;
        exit.consumeItem = placement.consumeItem;
        exit.nextLevel = placement.nextLevel;
        exit.sprite =
            simple_platformer::Sprite{textureId, {{48.0F, 216.0F}, {16.0F, 32.0F}}, {16.0F, 32.0F}};
        return exit;
    }

    simple_platformer::GameLevel composeLevel(
        const simple_platformer::ExampleLevelData& data,
        int textureId)
    {
        simple_platformer::World world(simple_platformer::makeExampleItems(textureId));
        for (const simple_platformer::ExampleActorPlacement& placement : data.actors)
        {
            world.addActor(makeActor(textureId, placement));
        }
        for (const simple_platformer::ExamplePickupPlacement& placement : data.pickups)
        {
            world.addPickup(makePickup(placement));
        }
        world.setExit(makeExit(textureId, data.exit));

        return {data.number, makeMap(data), std::move(world), data.playerSpawnFeet};
    }
}

namespace simple_platformer
{
    GameLevel makeGameLevel(const LevelCatalog& catalog, int levelNumber, int textureId)
    {
        const ExampleLevelData data = loadExampleLevelData(levelPath(catalog, levelNumber));
        if (data.number != levelNumber)
        {
            throw std::invalid_argument("The level file contains the wrong level number");
        }
        return composeLevel(data, textureId);
    }

    Actor makeExamplePlayer(int textureId)
    {
        AnimationSet animations = makePlayerAnimations();

        Actor player;
        player.body.bounds.size = {12.0F, 20.0F};
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

}
