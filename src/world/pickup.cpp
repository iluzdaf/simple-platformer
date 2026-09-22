#include "simple_platformer/world/pickup.hpp"

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/world/world.hpp"
#include "simple_platformer/world/world_requests.hpp"

namespace simple_platformer
{
    void validatePickup(const Pickup& pickup)
    {
        if (!simple_platformer::isFinite(pickup.body.bounds.position) ||
            !simple_platformer::isFinite(pickup.body.bounds.size) ||
            !simple_platformer::isFinite(pickup.body.velocity) ||
            pickup.body.bounds.size.x <= 0.0F || pickup.body.bounds.size.y <= 0.0F)
        {
            throw std::invalid_argument("Pickups require a finite body with positive-sized bounds");
        }
        if (pickup.stack.quantity <= 0)
        {
            throw std::invalid_argument("Pickups require a positive quantity");
        }
    }

    void World::addPickup(Pickup pickup)
    {
        validatePickup(pickup);
        itemDefinition(pickup.stack.item);
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

    void updatePickupMovement(const TileMap& map, World& world, float deltaTime)
    {
        requireSeconds(deltaTime, "Pickup movement time step");
        for (Pickup& pickup : world.pickups())
        {
            applyGravity(pickup.body, DefaultGravity, DefaultMaximumFallSpeed, deltaTime);
            moveBody(map, pickup.body, deltaTime);
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
            if (overlaps(player->body.bounds, world.pickups()[index].body.bounds))
            {
                requests.collectPickup(index);
            }
        }
    }
}
