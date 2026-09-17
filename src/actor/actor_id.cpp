#include "simple_platformer/actor/actor_id.hpp"

namespace simple_platformer
{
    bool operator==(ActorId left, ActorId right)
    {
        return left.value == right.value;
    }

    bool operator!=(ActorId left, ActorId right)
    {
        return !(left == right);
    }

    bool isValid(ActorId id)
    {
        return id.value != 0;
    }
}
