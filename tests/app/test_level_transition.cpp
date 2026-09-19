#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_message.hpp>

#include <cstddef>

#include "game/example_game.hpp"
#include "game/level_catalog.hpp"
#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/inventory/inventory.hpp"

namespace
{
    constexpr int MaximumSimulationTicks = 12000;

    bool sameHealth(simple_platformer::Health left, simple_platformer::Health right)
    {
        return left.current == right.current && left.maximum == right.maximum;
    }

    bool sameInventory(
        const simple_platformer::Inventory& left,
        const simple_platformer::Inventory& right)
    {
        const auto& leftSlots = left.slots();
        const auto& rightSlots = right.slots();
        if (leftSlots.size() != rightSlots.size())
        {
            return false;
        }

        for (std::size_t slot = 0; slot < leftSlots.size(); ++slot)
        {
            const auto& leftSlot = leftSlots[slot];
            const auto& rightSlot = rightSlots[slot];
            if (leftSlot.has_value() != rightSlot.has_value())
            {
                return false;
            }
            if (leftSlot.has_value() && (leftSlot.value().item != rightSlot.value().item ||
                                         leftSlot.value().quantity != rightSlot.value().quantity))
            {
                return false;
            }
        }

        return true;
    }
}

TEST_CASE(
    "The example carries progress across levels and restarts after the final exit",
    "[level-transition]")
{
    simple_platformer::ExampleGame game(
        0, simple_platformer::loadLevelCatalog("tests/fixtures/levels/levels.json"));
    const auto initialHealth = game.playerHealth();
    const auto initialInventory = game.playerInventory();
    const int initialLevel = game.levelNumber();
    simple_platformer::InputIntentions intentions;
    intentions.direction.x = 1.0F;
    bool changedLevel = false;

    for (int tick = 0; tick < MaximumSimulationTicks && !game.complete(); ++tick)
    {
        const int previousLevel = game.levelNumber();
        const auto previousHealth = game.playerHealth();
        const auto previousInventory = game.playerInventory();
        game.update(intentions, 1.0F / 60.0F);
        if (game.levelNumber() != previousLevel)
        {
            changedLevel = true;
            REQUIRE(sameHealth(game.playerHealth(), previousHealth));
            REQUIRE(sameInventory(game.playerInventory(), previousInventory));
        }
    }

    REQUIRE(changedLevel);
    REQUIRE(game.complete());
    const auto health = game.playerHealth();
    game.update(intentions, 1.0F / 60.0F);
    REQUIRE(game.complete());
    REQUIRE(sameHealth(game.playerHealth(), health));
    REQUIRE(game.levelExitScreenPosition().has_value());

    game.restart();
    REQUIRE_FALSE(game.complete());
    REQUIRE(game.levelNumber() == initialLevel);
    REQUIRE(sameHealth(game.playerHealth(), initialHealth));
    REQUIRE(sameInventory(game.playerInventory(), initialInventory));
}
