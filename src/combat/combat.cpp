#include "simple_platformer/combat/combat.hpp"

namespace simple_platformer
{
    bool areOpponents(Team first, Team second)
    {
        return (first == Team::Player && second == Team::Enemy) ||
               (first == Team::Enemy && second == Team::Player);
    }
}
