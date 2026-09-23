#include "application.hpp"

#include <cstdlib>
#include <cstddef>
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
#include <implot.h>

#include "content/level_catalog.hpp"
#include "debug/debug_overlay_ui.hpp"
#include "game/game.hpp"
#include "graphics/display_viewport.hpp"
#include "graphics/sprite_renderer.hpp"
#include "ui/completion_ui.hpp"
#include "ui/exit_hint_ui.hpp"
#include "ui/health_hud_ui.hpp"
#include "ui/inventory_ui.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/render/render_scene.hpp"
#include "simple_platformer/timing/fixed_step.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "simple_platformer/timing/stopwatch.hpp"

namespace simple_platformer
{
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
                ImPlot::CreateContext();
                ImGui::StyleColorsDark();

                if (!ImGui_ImplGlfw_InitForOpenGL(window, true))
                {
                    destroyContexts();
                    throw std::runtime_error("ImGui could not start its GLFW backend");
                }
                if (!ImGui_ImplOpenGL3_Init("#version 330 core"))
                {
                    ImGui_ImplGlfw_Shutdown();
                    destroyContexts();
                    throw std::runtime_error("ImGui could not start its OpenGL backend");
                }
            }

            ~ImGuiSession()
            {
                ImGui_ImplOpenGL3_Shutdown();
                ImGui_ImplGlfw_Shutdown();
                destroyContexts();
            }

            static void destroyContexts()
            {
                ImPlot::DestroyContext();
                ImGui::DestroyContext();
            }

            ImGuiSession(const ImGuiSession&) = delete;
            ImGuiSession& operator=(const ImGuiSession&) = delete;
        };

        struct ApplicationContext
        {
            InputState input;
            glm::vec2 aimDirection = {1.0F, 0.0F};
            bool showDebugOverlay = false;
            bool inventoryOpen = false;
            bool inventoryToggled = false;
            bool restartRequested = false;
        };

        std::optional<InputButton> buttonForKey(int key)
        {
            switch (key)
            {
            case GLFW_KEY_A:
            case GLFW_KEY_LEFT:
                return InputButton::Left;
            case GLFW_KEY_D:
            case GLFW_KEY_RIGHT:
                return InputButton::Right;
            case GLFW_KEY_W:
            case GLFW_KEY_UP:
            case GLFW_KEY_SPACE:
                return InputButton::Jump;
            case GLFW_KEY_DOWN:
                return InputButton::Down;
            default:
                return std::nullopt;
            }
        }

        void handleKey(GLFWwindow* window, int key, int, int action, int)
        {
            auto* context = static_cast<ApplicationContext*>(glfwGetWindowUserPointer(window));

            if (key == GLFW_KEY_Q && action == GLFW_PRESS)
            {
                context->inventoryOpen = !context->inventoryOpen;
                context->inventoryToggled = true;
                context->input = {};
                return;
            }
            if (key == GLFW_KEY_R && action == GLFW_PRESS)
            {
                context->restartRequested = true;
                return;
            }

            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
            {
                context->showDebugOverlay = !context->showDebugOverlay;
                return;
            }

            if (action != GLFW_PRESS && action != GLFW_RELEASE)
            {
                return;
            }

            const std::optional<InputButton> button = buttonForKey(key);
            if (!button.has_value())
            {
                return;
            }

            context->input.setButton(*button, action == GLFW_PRESS);
        }

        void handleMouseButton(GLFWwindow* window, int button, int action, int)
        {
            if (button != GLFW_MOUSE_BUTTON_LEFT ||
                (action != GLFW_PRESS && action != GLFW_RELEASE))
            {
                return;
            }

            auto* context = static_cast<ApplicationContext*>(glfwGetWindowUserPointer(window));
            context->input.setButton(InputButton::PrimaryAttack, action == GLFW_PRESS);
        }
    }

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
        const TextureView atlasTexture = renderer.textureView(atlas);
        Game game(atlas, loadLevelCatalog("assets/levels.json"));
        FixedStep fixedStep;
        FrameHistory frameHistory;
        Stopwatch frameClock;

        while (glfwWindowShouldClose(window.get()) == GLFW_FALSE)
        {
            glfwPollEvents();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            bool gameRestarted = false;
            if (context.restartRequested)
            {
                if (game.complete())
                {
                    game.restart();
                    context.inventoryOpen = false;
                    context.input = {};
                    context.aimDirection = {1.0F, 0.0F};
                    gameRestarted = true;
                }
                context.restartRequested = false;
            }

            int windowWidth = 0;
            int windowHeight = 0;
            int framebufferWidth = 0;
            int framebufferHeight = 0;
            double cursorX = 0.0;
            double cursorY = 0.0;
            glfwGetWindowSize(window.get(), &windowWidth, &windowHeight);
            glfwGetFramebufferSize(window.get(), &framebufferWidth, &framebufferHeight);
            glfwGetCursorPos(window.get(), &cursorX, &cursorY);
            const std::optional<WindowViewport> windowViewport = makeWindowViewport(
                {windowWidth, windowHeight}, {framebufferWidth, framebufferHeight});
            bool inventoryButtonClicked = false;
            if (windowViewport.has_value() && !game.complete())
            {
                inventoryButtonClicked = drawInventoryButton(atlasTexture, *windowViewport);
            }
            if (inventoryButtonClicked)
            {
                context.inventoryOpen = !context.inventoryOpen;
                context.inventoryToggled = true;
            }
            const std::optional<glm::vec2> internalCursor = windowToInternal(
                {static_cast<float>(cursorX), static_cast<float>(cursorY)},
                {windowWidth, windowHeight},
                {framebufferWidth, framebufferHeight});
            const bool mouseAvailable = internalCursor.has_value() &&
                                        !ImGui::GetIO().WantCaptureMouse && !context.inventoryOpen;
            if (!mouseAvailable)
            {
                context.input.clearButton(InputButton::PrimaryAttack);
            }

            FrameProfile profile;
            profile.frameSeconds = frameClock.lapSeconds();
            const bool paused = context.inventoryOpen || game.complete();
            if (paused || context.inventoryToggled || gameRestarted)
            {
                // Discard paused time and input edges, including a UI click on the closing frame.
                fixedStep.reset();
                context.input = {};
                context.inventoryToggled = false;
            }
            else
            {
                if (ImGui::GetIO().WantCaptureKeyboard)
                {
                    context.input = {};
                }
                const Stopwatch simulationWatch;
                const FixedStepResult stepped = fixedStep.advance(
                    profile.frameSeconds,
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
                        game.update(intentions, deltaTime, &profile);
                    });
                profile.simulationTicks = static_cast<int>(stepped.updates);
                profile.simulationSeconds = simulationWatch.elapsedSeconds();
            }

            const Stopwatch sceneWatch;
            const RenderScene scene = game.buildScene();
            profile.sceneSeconds = sceneWatch.elapsedSeconds();
            const Stopwatch renderWatch;
            renderer.render(scene, framebufferWidth, framebufferHeight);
            profile.renderSeconds = renderWatch.elapsedSeconds();

            const Stopwatch interfaceWatch;
            if (windowViewport.has_value())
            {
                drawHealthHud(game.playerHealth(), atlasTexture, *windowViewport);
                if (!game.complete())
                {
                    drawLockedExitHint(game, atlasTexture, *windowViewport);
                }
                drawLevelCompletion(game, *windowViewport);
            }
            if (context.inventoryOpen && !game.complete() && windowViewport.has_value())
            {
                const std::optional<std::size_t> slotToUse =
                    drawInventory(game, atlasTexture, *windowViewport);
                if (slotToUse.has_value())
                {
                    game.useInventoryItem(*slotToUse);
                }
            }
            profile.interfaceSeconds = interfaceWatch.elapsedSeconds();
            frameHistory.push(profile);

            if (context.showDebugOverlay)
            {
                drawDebugOverlay(
                    game.debugOverlay(
                        static_cast<float>(atlasTexture.width),
                        static_cast<float>(fixedStep.stepSeconds())),
                    windowViewport);
                drawFrameProfile(frameHistory);
            }
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window.get());
        }

        return EXIT_SUCCESS;
    }
}
