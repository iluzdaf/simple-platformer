#pragma once

namespace simple_platformer
{
    // How a step is travelled. Fly is the one kind a flying actor uses; the rest are a
    // platformer's.
    enum class Traversal
    {
        Fly,
        Walk,
        Fall,
        Jump,
        Climb
    };
}
