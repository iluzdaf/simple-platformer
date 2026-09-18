#include "example_content.hpp"

#include "example_animations.hpp"
#include "example_items.hpp"
#include "level_catalog.hpp"
#include "example_level_data.hpp"
#include "tile_catalog.hpp"

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
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    namespace
    {
        constexpr float ExamplePickupSideLength = 16.0F;
        constexpr float ZombieMaximumSpeed = 60.0F;
        constexpr float ZombieNoticeDistance = 80.0F;
        constexpr float ZombieBiteWindupDuration = 0.30F;
        constexpr float ZombieBiteActiveDuration = 0.10F;
        constexpr float ZombieBiteRecoveryDuration = 0.60F;
        constexpr int BatHealth = 1;
        constexpr float BatNoticeDistance = 56.0F;
        constexpr float BatForgetAfter = 0.50F;
        constexpr float PlayerProjectileLifetime = 0.90F;
        constexpr int ZombieSoldierHealth = 2;
        constexpr float ZombieSoldierProjectileSpeed = 140.0F;
        constexpr float ZombieSoldierProjectileLifetime = 0.75F;
        constexpr float ZombieSoldierShootDuration = 0.30F;
        constexpr float ZombieSoldierRecoveryDuration = 0.80F;

        Animator makeAnimator(AnimationSet animations)
        {
            Animator animator;
            animator.animationSet = std::move(animations);
            return animator;
        }

        Sprite makeActorSprite(int textureId, const AnimationSet& animations)
        {
            const AnimationClip& idle = clipFor(animations, AnimationName::Idle);
            return {textureId, idle.frames.front(), idle.frames.front().size};
        }

        RangedWeapon makeRangedWeapon(int textureId, Team team)
        {
            RangedWeapon weapon;
            const SpriteRegion region = team == Team::Player
                                            ? SpriteRegion{{32.0F, 192.0F}, {8.0F, 4.0F}}
                                            : SpriteRegion{{64.0F, 192.0F}, {8.0F, 4.0F}};
            weapon.projectileSprite = {textureId, region, region.size};
            return weapon;
        }

        RangedWeapon makePlayerRangedWeapon(int textureId)
        {
            RangedWeapon weapon = makeRangedWeapon(textureId, Team::Player);
            weapon.projectileLifetime = PlayerProjectileLifetime;
            return weapon;
        }

        RangedWeapon makeZombieSoldierRangedWeapon(int textureId)
        {
            RangedWeapon weapon = makeRangedWeapon(textureId, Team::Enemy);
            weapon.projectileSpeed = ZombieSoldierProjectileSpeed;
            weapon.projectileLifetime = ZombieSoldierProjectileLifetime;
            weapon.shootDuration = ZombieSoldierShootDuration;
            weapon.recoveryDuration = ZombieSoldierRecoveryDuration;
            return weapon;
        }

        Actor makeNpc(
            int textureId,
            glm::vec2 spawnFeet,
            glm::vec2 bodySize,
            SpriteAnchor spriteAnchor,
            std::optional<Patrol> patrol,
            AnimationSet animations)
        {
            Actor npc;
            npc.body.bounds.size = bodySize;
            placeFeetAt(npc.body.bounds, spawnFeet);
            npc.facing = Facing::Left;
            npc.sprite = makeActorSprite(textureId, animations);
            npc.sprite->anchor = spriteAnchor;
            npc.animator = makeAnimator(std::move(animations));
            npc.health = Health{3, 3};
            npc.team = Team::Enemy;
            npc.brain = NpcBrain{};
            npc.senses = NpcSenses{};
            npc.patrol = patrol;
            npc.pathFollower = PathFollower{};
            return npc;
        }

        Actor makeZombie(int textureId, glm::vec2 spawnFeet, std::optional<Patrol> patrol)
        {
            Actor npc = makeNpc(
                textureId,
                spawnFeet,
                {12.0F, 20.0F},
                SpriteAnchor::BodyFeet,
                patrol,
                makeZombieAnimations());
            npc.platformerMovement = PlatformerMovement{};
            npc.platformerMovement->grounded = true;
            npc.platformerMovement->config.maximumSpeed = ZombieMaximumSpeed;
            NpcSenses zombieSenses;
            zombieSenses.noticeDistance = ZombieNoticeDistance;
            npc.senses = zombieSenses;
            npc.bite = BiteAttack{};
            npc.bite->windupDuration = ZombieBiteWindupDuration;
            npc.bite->activeDuration = ZombieBiteActiveDuration;
            npc.bite->recoveryDuration = ZombieBiteRecoveryDuration;
            return npc;
        }

        Actor makeBat(int textureId, glm::vec2 spawnFeet, std::optional<Patrol> patrol)
        {
            Actor npc = makeNpc(
                textureId,
                spawnFeet,
                {12.0F, 8.0F},
                SpriteAnchor::BodyCenter,
                patrol,
                makeBatAnimations());
            npc.flyingMovement = FlyingMovement{};
            npc.bite = BiteAttack{};
            npc.health = Health{BatHealth, BatHealth};
            npc.senses = NpcSenses{BatNoticeDistance, BatForgetAfter};
            return npc;
        }

        Actor makeZombieSoldier(int textureId, glm::vec2 spawnFeet, std::optional<Patrol> patrol)
        {
            Actor npc = makeNpc(
                textureId,
                spawnFeet,
                {12.0F, 20.0F},
                SpriteAnchor::BodyFeet,
                patrol,
                makeZombieSoldierAnimations());
            npc.platformerMovement = PlatformerMovement{};
            npc.platformerMovement->grounded = true;
            npc.health = Health{ZombieSoldierHealth, ZombieSoldierHealth};
            npc.rangedWeapon = makeZombieSoldierRangedWeapon(textureId);
            return npc;
        }

        Actor makeActor(int textureId, const ExampleActorPlacement& placement)
        {
            switch (placement.type)
            {
            case ExampleActorType::Zombie:
                return makeZombie(textureId, placement.spawnFeet, placement.patrol);
            case ExampleActorType::Bat:
                return makeBat(textureId, placement.spawnFeet, placement.patrol);
            case ExampleActorType::ZombieSoldier:
                return makeZombieSoldier(textureId, placement.spawnFeet, placement.patrol);
            }
            throw std::logic_error("Unknown example actor type");
        }

        Pickup makePickup(const ExamplePickupPlacement& placement)
        {
            Pickup pickup;
            pickup.bounds.size = {ExamplePickupSideLength, ExamplePickupSideLength};
            placeFeetAt(pickup.bounds, placement.spawnFeet);
            pickup.stack = placement.stack;
            return pickup;
        }

        LevelExit makeExit(int textureId, const ExampleExitPlacement& placement)
        {
            LevelExit exit;
            exit.bounds.size = {16.0F, 32.0F};
            placeFeetAt(exit.bounds, placement.spawnFeet);
            exit.requirement = placement.requirement;
            exit.consumeItem = placement.consumeItem;
            exit.nextLevel = placement.nextLevel;
            exit.sprite = Sprite{textureId, {{48.0F, 216.0F}, {16.0F, 32.0F}}, {16.0F, 32.0F}};
            return exit;
        }

        GameLevel composeLevel(
            const ExampleLevelData& data,
            const TileCatalog& tiles,
            int levelNumber,
            int textureId)
        {
            World world(makeExampleItems(textureId));
            for (const ExampleActorPlacement& placement : data.actors)
            {
                world.addActor(makeActor(textureId, placement));
            }
            for (const ExamplePickupPlacement& placement : data.pickups)
            {
                world.addPickup(makePickup(placement));
            }
            world.setExit(makeExit(textureId, data.exit));

            return {
                levelNumber,
                makeTileMap(data.mapRows, data.tileLegend, tiles),
                std::move(world),
                data.playerSpawnFeet};
        }
    }

    GameLevel makeGameLevel(const LevelCatalog& catalog, int levelNumber, int textureId)
    {
        const ExampleLevelData data = loadExampleLevelData(levelPath(catalog, levelNumber));
        const TileCatalog tiles = loadTileCatalog(catalog.levelDirectory / "tiles.json");
        return composeLevel(data, tiles, levelNumber, textureId);
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
        player.rangedWeapon = makePlayerRangedWeapon(textureId);
        return player;
    }

}
