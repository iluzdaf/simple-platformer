#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include "content/content_json.hpp"
#include "content/machine_catalog.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/npc/npc_state_machine.hpp"

using Catch::Matchers::ContainsSubstring;

TEST_CASE("Machine JSON keeps state order, expands from lists and reads holds", "[app][machines]")
{
    auto root =
        nlohmann::json::parse(simple_platformer::loadContentText("tests/fixtures/machines.json"));
    auto& machine = root["machines"]["test_machine"];
    machine["states"].push_back({{"name", "flee"}, {"does", "retreat"}});
    machine["transitions"].push_back(
        {{"from", {"rest", "hunt"}}, {"to", "flee"}, {"when", {{"targetTooClose", true}}}});
    const auto catalog = simple_platformer::parseMachineCatalog(root.dump(), "test machines");
    const simple_platformer::NpcStateMachine& parsed =
        simple_platformer::npcStateMachine(catalog, "test_machine");

    REQUIRE(parsed.name == "test_machine");
    REQUIRE(parsed.states.size() == 3);
    REQUIRE(parsed.states[0].name == "rest");
    REQUIRE(parsed.states[0].does == simple_platformer::NpcState::Idle);
    REQUIRE(parsed.states[2].does == simple_platformer::NpcState::Retreat);
    REQUIRE(parsed.transitions.size() == 4);
    REQUIRE(parsed.transitions[1].after == 0.5F);
    REQUIRE(parsed.transitions[1].when.at("targetKnown") == false);
    REQUIRE(parsed.transitions[2].from == "rest");
    REQUIRE(parsed.transitions[3].from == "hunt");
    REQUIRE(parsed.transitions[3].to == "flee");
    REQUIRE_THROWS_AS(
        simple_platformer::npcStateMachine(catalog, "missing"), std::invalid_argument);
}

TEST_CASE("Machine JSON rejects what the engine cannot run, naming where", "[app][machines]")
{
    auto root =
        nlohmann::json::parse(simple_platformer::loadContentText("tests/fixtures/machines.json"));
    auto& machine = root["machines"]["test_machine"];
    const char* expected = "";
    SECTION("Unknown activity")
    {
        machine["states"][0]["does"] = "sleep";
        expected = "machines.test_machine.states[0].does";
    }
    SECTION("An empty state name")
    {
        machine["states"][0]["name"] = "";
        expected = "machines.test_machine.states[0].name";
    }
    SECTION("Unknown fact")
    {
        machine["transitions"][0]["when"]["cornered"] = true;
        expected = "asks about \"cornered\"";
    }
    SECTION("A transition to a state the machine lacks")
    {
        machine["transitions"][0]["to"] = "pounce";
        expected = "leads to a state the machine lacks";
    }
    SECTION("A condition that is not a boolean")
    {
        machine["transitions"][0]["when"]["targetKnown"] = 1;
        expected = "transitions[0].when.targetKnown";
    }
    SECTION("An empty from list")
    {
        machine["transitions"][0]["from"] = nlohmann::json::array();
        expected = "at least one state name";
    }
    SECTION("Unknown field")
    {
        machine["start"] = "rest";
        expected = "machines.test_machine";
    }
    REQUIRE_THROWS_WITH(
        simple_platformer::parseMachineCatalog(root.dump(), "machines.json"),
        ContainsSubstring("machines.json") && ContainsSubstring(expected));
}
