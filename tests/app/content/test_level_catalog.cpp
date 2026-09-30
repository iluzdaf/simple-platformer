#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <glm/vec2.hpp>
#include "content/level_catalog.hpp"

TEST_CASE("Level catalog numbers reject narrowing and fields reject typos", "[app][content][json]")
{
    auto levelCatalogJson = nlohmann::json::parse(
        R"({"startLevel":1,"cameraDeadZone":[80,45],"levels":[{"number":1,"file":"one.json"}]})");
    SECTION("Above int range")
    {
        levelCatalogJson["startLevel"] = 4294967297LL;
    }
    SECTION("Below int range")
    {
        levelCatalogJson["levels"][0]["number"] = -4294967295LL;
    }
    SECTION("Top-level field typo")
    {
        levelCatalogJson["startLevell"] = 1;
    }
    SECTION("Entry typo")
    {
        levelCatalogJson["levels"][0]["fille"] = "two.json";
    }
    REQUIRE_THROWS_WITH(
        simple_platformer::parseLevelCatalog(levelCatalogJson.dump(), "levels.json"),
        Catch::Matchers::ContainsSubstring("levels.json:"));
}

TEST_CASE("A level catalog maps stable IDs to arbitrary file names", "[app][content][json]")
{
    const auto catalog = simple_platformer::parseLevelCatalog(
        R"({
            "startLevel": 10,
            "cameraDeadZone": [80, 45],
            "levels": [
                {"number": 10, "file": "opening.json"},
                {"number": 25, "file": "areas/final_room.json"}
            ]
        })",
        "test catalog",
        "levels");

    REQUIRE(catalog.startLevel == 10);
    REQUIRE(catalog.cameraDeadZone == glm::vec2{80.0F, 45.0F});
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
                R"({"startLevel": 2, "cameraDeadZone": [80, 45], "levels": [{"number": 1, "file": "one.json"}]})",
                "test catalog"),
            std::invalid_argument);
    }

    SECTION("level ID is duplicated")
    {
        REQUIRE_THROWS_AS(
            simple_platformer::parseLevelCatalog(
                R"({
                    "startLevel": 1,
                    "cameraDeadZone": [80, 45],
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
                R"({"startLevel": 1, "cameraDeadZone": [80, 45], "levels": [{"number": 1, "file": "../one.json"}]})",
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

TEST_CASE(
    "A level catalog's camera dead zone is positive and fits in the view",
    "[app][content][json]")
{
    auto levelCatalogJson = nlohmann::json::parse(
        R"({"startLevel":1,"cameraDeadZone":[80,45],"levels":[{"number":1,"file":"one.json"}]})");
    SECTION("Missing")
    {
        levelCatalogJson.erase("cameraDeadZone");
    }
    SECTION("Empty")
    {
        levelCatalogJson["cameraDeadZone"] = {0, 45};
    }
    SECTION("Wider than the view")
    {
        levelCatalogJson["cameraDeadZone"] = {321, 45};
    }
    SECTION("Taller than the view")
    {
        levelCatalogJson["cameraDeadZone"] = {80, 181};
    }
    REQUIRE_THROWS_WITH(
        simple_platformer::parseLevelCatalog(levelCatalogJson.dump(), "levels.json"),
        Catch::Matchers::ContainsSubstring("cameraDeadZone"));
}
