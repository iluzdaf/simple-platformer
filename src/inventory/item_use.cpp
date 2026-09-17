#include "simple_platformer/inventory/item_use.hpp"

#include <algorithm>
#include <cstddef>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/inventory/item.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    bool useItem(World& world, ActorId id, std::size_t slot)
    {
        Actor* actor = world.findActor(id);
        if (actor == nullptr || actor->life != LifeState::Alive || !actor->inventory.has_value() ||
            slot >= actor->inventory->slots().size())
        {
            return false;
        }
        const auto& selected = actor->inventory->slots()[slot];
        if (!selected.has_value())
        {
            return false;
        }
        const ItemDefinition& definition = world.itemDefinition(selected->item);
        switch (definition.effect)
        {
        case ItemEffect::None:
            return false;
        case ItemEffect::Heal:
            if (!actor->health.has_value() || actor->health->current >= actor->health->maximum)
            {
                return false;
            }
            actor->health->current +=
                std::min(definition.effectAmount, actor->health->maximum - actor->health->current);
            actor->inventory->removeFromSlot(slot, 1);
            return true;
        }
        return false;
    }
}
