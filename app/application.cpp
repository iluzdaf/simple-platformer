#include "application.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>

#include <glad/glad.h>

#include <glm/vec2.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "actor_debug_ui.hpp"
#include "example_game.hpp"
#include "graphics/display_viewport.hpp"
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

    class ImGuiSession
    {
    public:
        explicit ImGuiSession(GLFWwindow* window)
        {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGui::StyleColorsDark();

            if (!ImGui_ImplGlfw_InitForOpenGL(window, true))
            {
                ImGui::DestroyContext();
                throw std::runtime_error("ImGui could not start its GLFW backend");
            }
            if (!ImGui_ImplOpenGL3_Init("#version 330 core"))
            {
                ImGui_ImplGlfw_Shutdown();
                ImGui::DestroyContext();
                throw std::runtime_error("ImGui could not start its OpenGL backend");
            }
        }

        ~ImGuiSession()
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }

        ImGuiSession(const ImGuiSession&) = delete;
        ImGuiSession& operator=(const ImGuiSession&) = delete;
    };

    struct ApplicationContext
    {
        simple_platformer::InputState input;
        glm::vec2 aimDirection = {1.0F, 0.0F};
        bool showActorDebug = false;
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
        default:
            return std::nullopt;
        }
    }

    void handleKey(GLFWwindow* window, int key, int, int action, int)
    {
        auto* context = static_cast<ApplicationContext*>(glfwGetWindowUserPointer(window));

        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
        {
            context->showActorDebug = !context->showActorDebug;
            return;
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

        context->input.setButton(*button, action == GLFW_PRESS);
    }

    void handleMouseButton(GLFWwindow* window, int button, int action, int)
    {
        if (button != GLFW_MOUSE_BUTTON_LEFT || (action != GLFW_PRESS && action != GLFW_RELEASE))
        {
            return;
        }

        auto* context = static_cast<ApplicationContext*>(glfwGetWindowUserPointer(window));
        context->input.setButton(
            simple_platformer::InputButton::PrimaryAttack, action == GLFW_PRESS);
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
            glfwCreateWindow(960, 540, "Simple Platformer", nullptr, nullptr), glfwDestroyWindow);
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

        ApplicationContext context;
        glfwSetWindowUserPointer(window.get(), &context);
        glfwSetKeyCallback(window.get(), handleKey);
        glfwSetMouseButtonCallback(window.get(), handleMouseButton);
        const ImGuiSession imgui(window.get());

        SpriteRenderer renderer;
        const int atlas = renderer.loadTexture("assets/sprites.png");
        ExampleGame game(atlas);
        FixedStep fixedStep;
        double previousTime = glfwGetTime();

        while (glfwWindowShouldClose(window.get()) == GLFW_FALSE)
        {
            glfwPollEvents();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            const double currentTime = glfwGetTime();
            const double frameTime = currentTime - previousTime;
            previousTime = currentTime;

            int windowWidth = 0;
            int windowHeight = 0;
            int framebufferWidth = 0;
            int framebufferHeight = 0;
            double cursorX = 0.0;
            double cursorY = 0.0;
            glfwGetWindowSize(window.get(), &windowWidth, &windowHeight);
            glfwGetFramebufferSize(window.get(), &framebufferWidth, &framebufferHeight);
            glfwGetCursorPos(window.get(), &cursorX, &cursorY);
            const std::optional<glm::vec2> internalCursor = windowToInternal(
                {static_cast<float>(cursorX), static_cast<float>(cursorY)},
                {windowWidth, windowHeight},
                {framebufferWidth, framebufferHeight});
            const bool mouseAvailable =
                internalCursor.has_value() && !ImGui::GetIO().WantCaptureMouse;
            if (!mouseAvailable)
            {
                context.input.clearButton(InputButton::PrimaryAttack);
            }

            fixedStep.advance(
                frameTime,
                [&](float deltaTime)
                {
                    InputIntentions intentions = context.input.consumeIntentions();
                    if (mouseAvailable && internalCursor.has_value())
                    {
                        const glm::vec2 aimDirection = game.playerAimDirection(*internalCursor);
                        if (aimDirection != glm::vec2{0.0F, 0.0F})
                        {
                            context.aimDirection = aimDirection;
                        }
                    }
                    else
                    {
                        intentions.primaryAttackPressed = false;
                    }
                    intentions.aimDirection = context.aimDirection;
                    game.update(intentions, deltaTime);
                });

            const simple_platformer::RenderScene scene = game.buildScene();
            renderer.render(scene, framebufferWidth, framebufferHeight);
            if (context.showActorDebug)
            {
                drawActorDebugUi(
                    game.actorDebugScene(), window.get(), framebufferWidth, framebufferHeight);
            }
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window.get());
        }

        return EXIT_SUCCESS;
    }
}
