#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <optional>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "content/actor_catalog.hpp"
#include "content/actor_definition.hpp"
#include "content/animation_catalog.hpp"
#include "content/machine_catalog.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"
#include "simple_platformer/movement/surface_climb.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "support/actor_components.hpp"

TEST_CASE("Actor JSON accepts custom names and configures component choices", "[app][actors][json]")
{
    const auto animations =
        simple_platformer::loadAnimationCatalog("tests/fixtures/catalogs/animations.json");
    const auto machines =
        simple_platformer::loadMachineCatalog("tests/fixtures/catalogs/machines.json");
    const auto catalog = simple_platformer::parseActorCatalog(
        R"({
        "player":"hero", "actors":{
          "hero":{"bodySize":[12,20],"platformer":{"jumpSpeed":210},"health":5,"inventorySlots":3},
          "scout":{"flying":{"speed":25},"team":"enemy","senses":{"noticeDistance":40,"searchDuration":3,"standoffDistance":30},
                   "tactic":"keepDistance",
                   "machine":"test_machine",
                   "bodySize":[8,6],"animations":"test_actor","spriteAnchor":"center","bite":{"damage":2}}
        }})",
        "test actors",
        animations,
        machines);
    REQUIRE(catalog.player == "hero");
    auto actor = simple_platformer::composeActor(
        simple_platformer::actorDefinition(catalog, "scout"),
        animations,
        7,
        {},
        std::nullopt,
        machines);
    REQUIRE(actor.machine.has_value());
    REQUIRE(
        simple_platformer::activeNpcMachineState(
            actor.machine.value_or(simple_platformer::NpcMachine{}))
            .name == "rest");
    REQUIRE(tests::flyingMovement(actor).speed == 25);
    REQUIRE(tests::bite(actor).damage == 2);
    REQUIRE(tests::senses(actor).searchDuration == 3);
    REQUIRE(tests::brain(actor).tactic == simple_platformer::NpcTactic::KeepDistance);
    REQUIRE(tests::senses(actor).standoffDistance == 30);
    REQUIRE(tests::sprite(actor).textureId == 7);
    REQUIRE(tests::sprite(actor).anchor == simple_platformer::SpriteAnchor::BodyCenter);
    REQUIRE_FALSE(actor.platformerMovement.has_value());
    REQUIRE_THROWS_AS(
        simple_platformer::actorDefinition(catalog, "missing"), std::invalid_argument);
}

TEST_CASE("Actor JSON configures climbing without exposing attachment state", "[app][actors][json]")
{
    const auto catalog = simple_platformer::parseActorCatalog(
        R"({"player":"hero","actors":{"hero":{"bodySize":[12,12],"platformer":{},
        "surfaceClimb":{"speed":75},"health":2,"inventorySlots":1}}})",
        "actors.json",
        {});

    auto actor =
        simple_platformer::composeActor(simple_platformer::actorDefinition(catalog, "hero"), {}, 0);
    REQUIRE(tests::surfaceClimb(actor).config.speed == 75.0F);
    REQUIRE(tests::surfaceClimb(actor).surface == simple_platformer::ClimbSurface::None);
}

TEST_CASE("Climbing requires platformer movement and positive speed", "[app][actors][json]")
{
    auto actorJson = nlohmann::json::parse(
        R"({"player":"hero","actors":{"hero":{"bodySize":[12,12],"platformer":{},
        "surfaceClimb":{"speed":75},"health":2,"inventorySlots":1}}})");
    SECTION("Flying actor")
    {
        actorJson["actors"]["hero"].erase("platformer");
        actorJson["actors"]["hero"]["flying"] = {{"speed", 60}};
    }
    SECTION("Invalid speed")
    {
        actorJson["actors"]["hero"]["surfaceClimb"]["speed"] = 0;
    }
    SECTION("Runtime attachment state")
    {
        actorJson["actors"]["hero"]["surfaceClimb"]["surface"] = "ceiling";
    }

    REQUIRE_THROWS_WITH(
        simple_platformer::parseActorCatalog(actorJson.dump(), "actors.json", {}),
        Catch::Matchers::ContainsSubstring("actors.json:"));
}

TEST_CASE(
    "Actor JSON rejects malformed and invalid definitions including unused ones",
    "[app][actors][json]")
{
    auto actorJson = nlohmann::json::parse(
        R"({"player":"hero","actors":{"hero":{"bodySize":[12,20],"platformer":{},"health":3,"inventorySlots":2}}})");
    SECTION("Missing body size")
    {
        actorJson["actors"]["hero"].erase("bodySize");
    }
    SECTION("Missing player reference")
    {
        actorJson["player"] = "missing";
    }
    SECTION("Unknown animation")
    {
        actorJson["actors"]["hero"]["animations"] = "missing";
    }
    SECTION("Unused actor references an unknown animation")
    {
        actorJson["actors"]["unused"] = {{"flying", {{"speed", 25}}}, {"animations", "missing"}};
    }
    SECTION("Fractional health")
    {
        actorJson["actors"]["hero"]["health"] = 1.5;
    }
    SECTION("Boolean speed")
    {
        actorJson["actors"]["hero"]["platformer"]["maximumSpeed"] = true;
    }
    SECTION("Unknown field")
    {
        actorJson["actors"]["hero"]["heath"] = 3;
    }
    SECTION("Runtime state")
    {
        actorJson["actors"]["hero"]["brain"] = {};
    }
    SECTION("Unknown tactic")
    {
        actorJson["actors"]["hero"]["senses"] = {};
        actorJson["actors"]["hero"]["tactic"] = "ambusher";
    }
    SECTION("Unused definition")
    {
        actorJson["actors"]["unused"] = {{"flying", {{"speed", -1}}}};
    }
    REQUIRE_THROWS_WITH(
        simple_platformer::parseActorCatalog(actorJson.dump(), "actors.json", {}),
        Catch::Matchers::ContainsSubstring("actors.json:"));
}
