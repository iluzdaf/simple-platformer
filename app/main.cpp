#include <cstdlib>
#include <cstddef>
#include <iostream>

#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/physics/body.hpp"
#include "simple_platformer/physics/collision.hpp"
#include "simple_platformer/timing/fixed_step.hpp"
#include "simple_platformer/world/tile_map.hpp"

int main()
{
    simple_platformer::FixedStep clock;
    std::size_t simulatedUpdates = 0;

    const simple_platformer::FixedStepResult result =
        clock.advance(1.0 / 30.0, [&simulatedUpdates](float) { ++simulatedUpdates; });

    if (result.updates != 2 || simulatedUpdates != 2)
    {
        std::cerr << "Simple Platformer fixed-step smoke check failed\n";
        return EXIT_FAILURE;
    }

    const simple_platformer::TileMap map =
        simple_platformer::TileMap::fromAscii({"....", "....", "####"});
    simple_platformer::Aabb bounds{{4.0F, 0.0F}, {12.0F, 12.0F}};
    const simple_platformer::CollisionContacts contacts =
        simple_platformer::moveAndCollide(map, bounds, {0.0F, 100.0F});

    if (!contacts.ground || bounds.position.y != 20.0F)
    {
        std::cerr << "Simple Platformer tile collision smoke check failed\n";
        return EXIT_FAILURE;
    }

    simple_platformer::InputState input;
    input.setButton(simple_platformer::InputButton::Right, true);
    input.setButton(simple_platformer::InputButton::Jump, true);

    simple_platformer::Body player{{{4.0F, 20.0F}, {12.0F, 12.0F}}, {0.0F, 0.0F}};
    simple_platformer::PlatformerMovement movement;
    movement.grounded = true;
    simple_platformer::Facing facing = simple_platformer::Facing::Left;
    simple_platformer::updatePlatformerMovement(
        map,
        player,
        movement,
        input.consumeIntentions(),
        facing,
        static_cast<float>(simple_platformer::FixedDeltaSeconds));

    if (player.velocity.x <= 0.0F || player.velocity.y >= 0.0F ||
        facing != simple_platformer::Facing::Right)
    {
        std::cerr << "Simple Platformer movement smoke check failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "Simple Platformer Phase 3 ready: fixed updates, tile collision, input, and "
                 "player movement verified\n";
    return EXIT_SUCCESS;
}
