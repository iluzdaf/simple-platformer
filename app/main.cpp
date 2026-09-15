#include <cstdlib>
#include <cstddef>
#include <iostream>

#include "simple_platformer/timing/fixed_step.hpp"

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

    std::cout << "Simple Platformer Phase 1 ready: " << simulatedUpdates
              << " fixed updates at 60 Hz\n";
    return EXIT_SUCCESS;
}
