#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <stdexcept>

#include "game/level_catalog.hpp"

TEST_CASE("A level catalog maps stable IDs to arbitrary file names", "[app][content][json]")
{
    const auto catalog = simple_platformer::parseLevelCatalog(
        R"({
            "startLevel": 10,
            "levels": [
                {"number": 10, "file": "opening.json"},
                {"number": 25, "file": "areas/final_room.json"}
            ]
        })",
        "test catalog",
        "levels");

    REQUIRE(catalog.startLevel == 10);
    REQUIRE(catalog.levels.size() == 2);
    REQUIRE(
        simple_platformer::levelPath(catalog, 25) ==
        std::filesystem::path("levels/areas/final_room.json"));
    REQUIRE_THROWS_AS(simple_platformer::levelPath(catalog, 1), std::invalid_argument);
}

TEST_CASE("A level catalog rejects ambiguous or unsafe entries", "[app][content][json]")
{
    SECTION("start level is not listed")
    {
        REQUIRE_THROWS_AS(
            simple_platformer::parseLevelCatalog(
                R"({"startLevel": 2, "levels": [{"number": 1, "file": "one.json"}]})",
                "test catalog"),
            std::invalid_argument);
    }

    SECTION("level ID is duplicated")
    {
        REQUIRE_THROWS_AS(
            simple_platformer::parseLevelCatalog(
                R"({
                    "startLevel": 1,
                    "levels": [
                        {"number": 1, "file": "one.json"},
                        {"number": 1, "file": "another.json"}
                    ]
                })",
                "test catalog"),
            std::invalid_argument);
    }

    SECTION("file escapes the level directory")
    {
        REQUIRE_THROWS_AS(
            simple_platformer::parseLevelCatalog(
                R"({"startLevel": 1, "levels": [{"number": 1, "file": "../one.json"}]})",
                "test catalog"),
            std::invalid_argument);
    }
}

TEST_CASE("A missing level catalog is rejected at the file boundary", "[app][content][json]")
{
    REQUIRE_THROWS_AS(
        simple_platformer::loadLevelCatalog("tests/fixtures/levels/does_not_exist.json"),
        std::invalid_argument);
}
