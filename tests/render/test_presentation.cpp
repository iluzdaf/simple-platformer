#include <catch2/catch_test_macros.hpp>

#include <optional>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/presentation.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "simple_platformer/world/world.hpp"
#include "support/actor_builder.hpp"
#include "support/actor_components.hpp"
#include "support/add_player.hpp"
#include "support/tile_map_builder.hpp"
#include "support/animator.hpp"

TEST_CASE(
    "Presenting the world animates actors and fades cover in one call",
    "[render][presentation]")
{
    const simple_platformer::TileMap map = tests::TileMapBuilder({"......", "...cc.", "......"})
                                               .where('c', tests::Tile().blocksSight());
    simple_platformer::World world;
    tests::addPlayer(world, tests::ActorBuilder::sized({12.0F, 12.0F}).inCell({0, 1}).walking());
    const simple_platformer::ActorId npc = world.addActor(tests::ActorBuilder::sized({12.0F, 12.0F})
                                                              .inCell({4, 1})
                                                              .flying(0.0F)
                                                              .withSprite({0, {}, {1.0F, 1.0F}})
                                                              .withAnimator(tests::fullAnimator()));

    simple_platformer::updateWorldPresentation(map, world, 0.0F);

    REQUIRE(tests::animator(world, npc).current == simple_platformer::AnimationName::Idle);
    REQUIRE(tests::actor(world, npc).screenVisibility == 0.0F);
}
