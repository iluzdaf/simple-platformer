#pragma once

#include <cstdint>

namespace simple_platformer
{
    struct ActorId
    {
        std::uint32_t value = 0;
    };

    bool operator==(ActorId left, ActorId right);
    bool operator!=(ActorId left, ActorId right);
    bool isValid(ActorId id);
}
