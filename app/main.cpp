#include <cstdlib>
#include <exception>
#include <iostream>

#include "application.hpp"

int main()
{
    try
    {
        return simple_platformer::runApplication();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Simple Platformer could not start: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
