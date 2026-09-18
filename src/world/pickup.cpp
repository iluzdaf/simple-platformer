#include "simple_platformer/world/pickup.hpp"

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"

namespace
{
    void validatePickup(
        const simple_platformer::World& world,
        const simple_platformer::Pickup& pickup)
    {
        if (!simple_platformer::isFinite(pickup.bounds.position) ||
            !simple_platformer::isFinite(pickup.bounds.size) || pickup.bounds.size.x <= 0.0F ||
            pickup.bounds.size.y <= 0.0F)
        {
            throw std::invalid_argument("Pickups require finite positive-sized bounds");
        }
        world.itemDefinition(pickup.stack.item);
        if (pickup.stack.quantity <= 0)
        {
            throw std::invalid_argument("Pickups require a positive quantity");
        }
    }
}

namespace simple_platformer
{
    void World::addPickup(Pickup pickup)
    {
        validatePickup(*this, pickup);
        pickupStorage.push_back(pickup);
    }

    const std::vector<Pickup>& World::pickups() const
    {
        return pickupStorage;
    }

    std::vector<Pickup>& World::pickups()
    {
        return pickupStorage;
    }

    void World::collectPickup(std::size_t index)
    {
        Actor* player = findActor(playerId());
        if (index >= pickupStorage.size() || player == nullptr ||
            player->life != LifeState::Alive || !player->inventory.has_value())
        {
            return;
        }
        Pickup& pickup = pickupStorage[index];
        const AddItemResult result =
            player->inventory->add(itemDefinition(pickup.stack.item), pickup.stack.quantity);
        pickup.stack.quantity = result.remaining;
        if (result.remaining == 0)
        {
            pickupStorage.erase(pickupStorage.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    void updatePickups(const World& world, WorldRequests& requests)
    {
        const Actor* player = world.findActor(world.playerId());
        if (player == nullptr || player->life != LifeState::Alive || !player->inventory.has_value())
        {
            return;
        }
        for (std::size_t index = 0; index < world.pickups().size(); ++index)
        {
            if (overlaps(player->body.bounds, world.pickups()[index].bounds))
            {
                requests.collectPickup(index);
            }
        }
    }
}
