#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <cstddef>
#include <stdexcept>

#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/inventory/item.hpp"

namespace
{
    simple_platformer::ItemDefinition makeItem(int id = 1, int maximumStack = 5)
    {
        return {id, "Item", {}, maximumStack};
    }

    const simple_platformer::ItemStack& stackAt(
        const simple_platformer::Inventory& inventory,
        std::size_t index)
    {
        const auto& slot = inventory.slots().at(index);
        if (!slot.has_value())
        {
            throw std::logic_error("Expected an occupied inventory slot");
        }
        return *slot;
    }
}

TEST_CASE("Inventory fills matching stacks before earlier empty slots", "[inventory]")
{
    simple_platformer::Inventory inventory(3);
    inventory.add(makeItem(2), 1);
    inventory.add(makeItem(), 2);
    inventory.remove(2, 1);
    const auto result = inventory.add(makeItem(), 5);
    REQUIRE(result.added == 5);
    REQUIRE(result.remaining == 0);
    REQUIRE(stackAt(inventory, 0).quantity == 2);
    REQUIRE(stackAt(inventory, 1).quantity == 5);
    REQUIRE_FALSE(inventory.slots()[2].has_value());
    REQUIRE(inventory.count(1) == 7);
}

TEST_CASE("Inventory reports the portion that cannot fit", "[inventory]")
{
    simple_platformer::Inventory inventory(2);
    const auto result = inventory.add(makeItem(), 12);
    REQUIRE(result.added == 10);
    REQUIRE(result.remaining == 2);
    REQUIRE(inventory.add(makeItem(2), 3).remaining == 3);
    REQUIRE(inventory.count(2) == 0);
    simple_platformer::Inventory noSlots(0);
    REQUIRE(noSlots.add(makeItem(), 4).remaining == 4);
}

TEST_CASE("Inventory removal spans stacks and fails without partial removal", "[inventory]")
{
    simple_platformer::Inventory inventory(2);
    inventory.add(makeItem(), 8);
    REQUIRE_FALSE(inventory.remove(1, 9));
    REQUIRE(inventory.count(1) == 8);
    REQUIRE(inventory.remove(1, 6));
    REQUIRE_FALSE(inventory.slots()[0].has_value());
    REQUIRE(stackAt(inventory, 1).quantity == 2);
    REQUIRE_FALSE(inventory.removeFromSlot(0, 1));
    REQUIRE_FALSE(inventory.removeFromSlot(20, 1));
    REQUIRE_FALSE(inventory.removeFromSlot(1, 3));
    REQUIRE(inventory.removeFromSlot(1, 2));
    REQUIRE(inventory.count(1) == 0);
}

TEST_CASE("Inventory rejects invalid quantities and definitions", "[inventory]")
{
    simple_platformer::Inventory inventory;
    REQUIRE_THROWS_AS(inventory.add(makeItem(), 0), std::invalid_argument);
    REQUIRE_THROWS_AS(inventory.add(makeItem(), -1), std::invalid_argument);
    REQUIRE_THROWS_AS(inventory.add(makeItem(0), 1), std::invalid_argument);
    REQUIRE_THROWS_AS(inventory.add(makeItem(1, 0), 1), std::invalid_argument);
    auto definition = makeItem();
    definition.name.clear();
    REQUIRE_THROWS_AS(inventory.add(definition, 1), std::invalid_argument);
    definition = makeItem();
    definition.effect = simple_platformer::ItemEffect::Heal;
    REQUIRE_THROWS_AS(inventory.add(definition, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(inventory.remove(1, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(inventory.removeFromSlot(0, -1), std::invalid_argument);
}

TEST_CASE("Inventory quantities cannot overflow when adding another stack", "[inventory]")
{
    simple_platformer::Inventory inventory(2);
    const auto definition = makeItem(1, std::numeric_limits<int>::max());
    inventory.add(definition, std::numeric_limits<int>::max());
    REQUIRE_THROWS_AS(inventory.add(definition, 1), std::invalid_argument);
    REQUIRE(inventory.count(1) == std::numeric_limits<int>::max());
}
