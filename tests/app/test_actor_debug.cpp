#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include <glm/vec2.hpp>

#include "actor_debug.hpp"
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/camera.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/world.hpp"

TEST_CASE("Actor debug data supports actors without presentation components", "[app][debug]")
{
    simple_platformer::Actor actor;
    actor.body.bounds = {{12.0F, 20.0F}, {8.0F, 10.0F}};
    actor.platformerMovement = simple_platformer::PlatformerMovement{};

    simple_platformer::World world;
    const simple_platformer::ActorId id = world.addActor(actor);
    const simple_platformer::Camera camera{{4.0F, 5.0F}, {320.0F, 180.0F}};

    const simple_platformer::ActorDebugScene debug =
        simple_platformer::makeActorDebugScene(world, camera, 128.0F);

    REQUIRE(debug.cameraPosition == camera.position);
    REQUIRE(debug.actors.size() == 1);
    REQUIRE(debug.actors.front().id == id);
    REQUIRE(debug.actors.front().kind == simple_platformer::ActorDebugKind::Actor);
    REQUIRE(debug.actors.front().collider.position == actor.body.bounds.position);
    REQUIRE(debug.actors.front().collider.size == actor.body.bounds.size);
    REQUIRE_FALSE(debug.actors.front().sprite.has_value());
    REQUIRE_FALSE(debug.actors.front().animation.has_value());
    REQUIRE_FALSE(debug.actors.front().npcState.has_value());
}

TEST_CASE("Actor debug data reports player presentation and NPC state", "[app][debug]")
{
    const simple_platformer::SpriteRegion region{{64.0F, 24.0F}, {32.0F, 24.0F}};
    simple_platformer::Animator animator;
    animator.current = simple_platformer::AnimationName::Move;
    animator.animationSet.clips.push_back(
        {simple_platformer::AnimationName::Move, {region}, 0.1F, true});

    simple_platformer::Actor player;
    player.body.bounds = {{32.0F, 196.0F}, {12.0F, 12.0F}};
    player.platformerMovement = simple_platformer::PlatformerMovement{};
    player.sprite = simple_platformer::Sprite{1, region, {32.0F, 24.0F}};
    player.animator = animator;

    simple_platformer::Actor npc;
    npc.body.bounds = {{80.0F, 196.0F}, {12.0F, 12.0F}};
    npc.platformerMovement = simple_platformer::PlatformerMovement{};
    simple_platformer::NpcBrain brain;
    brain.state = simple_platformer::NpcState::Chase;
    npc.brain = brain;
    npc.senses = simple_platformer::NpcSenses{};
    npc.pathFollower = simple_platformer::PathFollower{};

    simple_platformer::World world;
    const simple_platformer::ActorId playerId = world.addActor(player);
    const simple_platformer::ActorId npcId = world.addActor(npc);
    world.setPlayer(playerId, {38.0F, 208.0F});

    const simple_platformer::ActorDebugScene debug =
        simple_platformer::makeActorDebugScene(world, simple_platformer::Camera{}, 128.0F);

    REQUIRE(debug.actors.size() == 2);
    const simple_platformer::ActorDebugInfo& playerDebug = debug.actors.front();
    REQUIRE(playerDebug.kind == simple_platformer::ActorDebugKind::Player);
    REQUIRE(playerDebug.animation == simple_platformer::AnimationName::Move);
    REQUIRE(playerDebug.sprite.has_value());
    const simple_platformer::ActorSpriteDebugInfo spriteDebug =
        playerDebug.sprite.value_or(simple_platformer::ActorSpriteDebugInfo{});
    REQUIRE(spriteDebug.bounds.position == glm::vec2{22.0F, 184.0F});
    REQUIRE(spriteDebug.bounds.size == glm::vec2{32.0F, 24.0F});
    REQUIRE(spriteDebug.atlasFrame == 7);
    REQUIRE(spriteDebug.atlasPosition == region.position);

    const simple_platformer::ActorDebugInfo& npcDebug = debug.actors.back();
    REQUIRE(npcDebug.id == npcId);
    REQUIRE(npcDebug.kind == simple_platformer::ActorDebugKind::Npc);
    REQUIRE(npcDebug.npcState == simple_platformer::NpcState::Chase);
}

TEST_CASE("Actor debug data rejects an invalid atlas width", "[app][debug]")
{
    const simple_platformer::World world;
    const simple_platformer::Camera camera;

    REQUIRE_THROWS_AS(
        simple_platformer::makeActorDebugScene(world, camera, 0.0F), std::invalid_argument);
}
