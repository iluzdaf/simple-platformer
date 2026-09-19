#pragma once

namespace simple_platformer
{
    struct Actor;
    // Validates a newly composed actor before World assigns its identity.
    void validateActor(const Actor& actor);
}
