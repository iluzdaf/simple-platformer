#include "game_window.hpp"

#include <iostream>
#include <stdexcept>

#include <glad/glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/vec2.hpp>

#include "simple_platformer/math/coordinates.hpp"

namespace simple_platformer
{
    GameWindow::GlfwLibrary::GlfwLibrary()
    {
        glfwSetErrorCallback([](int, const char* description)
                             { std::cerr << "GLFW: " << description << '\n'; });
        if (glfwInit() == GLFW_FALSE)
        {
            throw std::runtime_error("GLFW could not start");
        }
    }

    GameWindow::GlfwLibrary::~GlfwLibrary()
    {
        glfwTerminate();
    }

    GameWindow::GameWindow(const char* title, glm::ivec2 size)
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

        window = glfwCreateWindow(size.x, size.y, title, nullptr, nullptr);
        if (window == nullptr)
        {
            throw std::runtime_error("GLFW could not create the game window");
        }

        glfwSetWindowSizeLimits(
            window, InternalWidth, InternalHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
        if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0)
        {
            throw std::runtime_error("GLAD could not load OpenGL");
        }
    }

    GameWindow::~GameWindow()
    {
        glfwDestroyWindow(window);
    }

    GLFWwindow* GameWindow::handle() const
    {
        return window;
    }

    bool GameWindow::shouldClose() const
    {
        return glfwWindowShouldClose(window) == GLFW_TRUE;
    }

    WindowReading GameWindow::read() const
    {
        WindowReading reading;
        double cursorX = 0.0;
        double cursorY = 0.0;
        glfwGetWindowSize(window, &reading.size.x, &reading.size.y);
        glfwGetFramebufferSize(window, &reading.framebufferSize.x, &reading.framebufferSize.y);
        glfwGetCursorPos(window, &cursorX, &cursorY);
        reading.cursor = {static_cast<float>(cursorX), static_cast<float>(cursorY)};
        return reading;
    }

    void GameWindow::present() const
    {
        glfwSwapBuffers(window);
    }
}
