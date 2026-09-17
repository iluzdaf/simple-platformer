#include "simple_platformer/inventory/item.hpp"

#include <stdexcept>

namespace simple_platformer
{
    void validateItemDefinition(const ItemDefinition& definition)
    {
        if (definition.id <= 0 || definition.name.empty() || definition.maximumStack <= 0 ||
            (definition.effect == ItemEffect::Heal && definition.effectAmount <= 0) ||
            (definition.effect == ItemEffect::None && definition.effectAmount != 0))
        {
            throw std::invalid_argument(
                "Item definitions require an ID, name and valid stack/effect");
        }
    }
}
