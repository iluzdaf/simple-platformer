#include "application.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>

#include <glad/glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "example_game.hpp"
#include "graphics/sprite_renderer.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/render/render_scene.hpp"
#include "simple_platformer/timing/fixed_step.hpp"

namespace
{
    class GlfwSession
    {
    public:
        GlfwSession()
        {
            if (glfwInit() == GLFW_FALSE)
            {
                throw std::runtime_error("GLFW could not start");
            }
        }

        ~GlfwSession()
        {
            glfwTerminate();
        }

        GlfwSession(const GlfwSession&) = delete;
        GlfwSession& operator=(const GlfwSession&) = delete;
    };

    std::optional<simple_platformer::InputButton> buttonForKey(int key)
    {
        switch (key)
        {
        case GLFW_KEY_LEFT:
            return simple_platformer::InputButton::Left;
        case GLFW_KEY_RIGHT:
            return simple_platformer::InputButton::Right;
        case GLFW_KEY_UP:
            return simple_platformer::InputButton::Up;
        case GLFW_KEY_DOWN:
            return simple_platformer::InputButton::Down;
        case GLFW_KEY_SPACE:
            return simple_platformer::InputButton::Jump;
        case GLFW_KEY_X:
            return simple_platformer::InputButton::PrimaryAttack;
        default:
            return std::nullopt;
        }
    }

    void handleKey(GLFWwindow* window, int key, int, int action, int)
    {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        if (action != GLFW_PRESS && action != GLFW_RELEASE)
        {
            return;
        }

        const std::optional<simple_platformer::InputButton> button = buttonForKey(key);
        if (!button.has_value())
        {
            return;
        }

        auto* input = static_cast<simple_platformer::InputState*>(glfwGetWindowUserPointer(window));
        input->setButton(*button, action == GLFW_PRESS);
    }
}

namespace simple_platformer
{
    int runApplication()
    {
        glfwSetErrorCallback([](int, const char* description)
                             { std::cerr << "GLFW: " << description << '\n'; });
        const GlfwSession glfw;

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

        using Window = std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)>;
        Window window(
            glfwCreateWindow(960, 540, "Simple Platformer - Phase 6", nullptr, nullptr),
            glfwDestroyWindow);
        if (!window)
        {
            throw std::runtime_error("GLFW could not create the game window");
        }

        glfwSetWindowSizeLimits(
            window.get(), InternalWidth, InternalHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);
        glfwMakeContextCurrent(window.get());
        glfwSwapInterval(1);
        if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0)
        {
            throw std::runtime_error("GLAD could not load OpenGL");
        }

        InputState input;
        glfwSetWindowUserPointer(window.get(), &input);
        glfwSetKeyCallback(window.get(), handleKey);

        SpriteRenderer renderer;
        const int atlas = renderer.loadTexture("assets/sprites.ppm");
        ExampleGame game(atlas);
        FixedStep fixedStep;
        double previousTime = glfwGetTime();

        while (glfwWindowShouldClose(window.get()) == GLFW_FALSE)
        {
            glfwPollEvents();
            const double currentTime = glfwGetTime();
            const double frameTime = currentTime - previousTime;
            previousTime = currentTime;

            fixedStep.advance(
                frameTime,
                [&](float deltaTime) { game.update(input.consumeIntentions(), deltaTime); });

            int framebufferWidth = 0;
            int framebufferHeight = 0;
            glfwGetFramebufferSize(window.get(), &framebufferWidth, &framebufferHeight);
            const simple_platformer::RenderScene scene = game.buildScene();
            renderer.render(scene, framebufferWidth, framebufferHeight);
            glfwSwapBuffers(window.get());
        }

        return EXIT_SUCCESS;
    }
}
