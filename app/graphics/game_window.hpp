#pragma once

#include <memory>

#include <glm/vec2.hpp>

struct GLFWwindow;

namespace simple_platformer
{
    // What the window reports at the start of a frame: its size in points, its
    // framebuffer in pixels, which differs on high-DPI displays, and the cursor in points.
    struct WindowReading
    {
        glm::ivec2 size = {0, 0};
        glm::ivec2 framebufferSize = {0, 0};
        glm::vec2 cursor = {0.0F, 0.0F};
    };

    // The game window and the OpenGL 3.3 core context it owns, for as long as it lives.
    // GLFW starts before the window and stops after it. The window never shrinks below
    // one internal image.
    class GameWindow
    {
    public:
        GameWindow(const char* title, glm::ivec2 size);
        ~GameWindow();

        GameWindow(const GameWindow&) = delete;
        GameWindow& operator=(const GameWindow&) = delete;

        GLFWwindow* handle() const;
        bool shouldClose() const;
        WindowReading read() const;
        // Shows what was drawn, waiting for the display's next refresh.
        void present() const;

    private:
        // Starts GLFW and stops it again. Declared before the window so GLFW is running
        // when the window is created and still running when it is destroyed, even when
        // the constructor throws part way.
        struct GlfwLibrary
        {
            GlfwLibrary();
            ~GlfwLibrary();
            GlfwLibrary(const GlfwLibrary&) = delete;
            GlfwLibrary& operator=(const GlfwLibrary&) = delete;
        };

        struct WindowDeleter
        {
            void operator()(GLFWwindow* target) const;
        };

        GlfwLibrary library;
        std::unique_ptr<GLFWwindow, WindowDeleter> window;
    };
}
