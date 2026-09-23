#include "application.hpp"

#include <cstdlib>
#include <optional>
#include <stdexcept>

#include <glm/vec2.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

#include "content/level_catalog.hpp"
#include "debug/debug_overlay_ui.hpp"
#include "debug/frame_profile_ui.hpp"
#include "game/game.hpp"
#include "graphics/display_viewport.hpp"
#include "graphics/game_window.hpp"
#include "graphics/sprite_renderer.hpp"
#include "ui/interface_ui.hpp"
#include "simple_platformer/input/input_state.hpp"
#include "simple_platformer/render/render_scene.hpp"
#include "simple_platformer/timing/fixed_step.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "simple_platformer/timing/stopwatch.hpp"

namespace simple_platformer
{
    namespace
    {
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
        const GameWindow window("Simple Platformer", {960, 540});
        ApplicationContext context;
        glfwSetWindowUserPointer(window.handle(), &context);
        glfwSetKeyCallback(window.handle(), handleKey);
        glfwSetMouseButtonCallback(window.handle(), handleMouseButton);
        const ImGuiSession imgui(window.handle());

        SpriteRenderer renderer;
        const int atlas = renderer.loadTexture("assets/sprites.png");
        const TextureView atlasTexture = renderer.textureView(atlas);
        Game game(atlas, loadLevelCatalog("assets/levels.json"));
        FixedStep fixedStep;
        FrameHistory frameHistory;
        Stopwatch frameClock;

        while (!window.shouldClose())
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

            const WindowReading reading = window.read();
            const std::optional<WindowViewport> windowViewport =
                makeWindowViewport(reading.size, reading.framebufferSize);

            FrameProfile profile;
            profile.frameSeconds = frameClock.lapSeconds();
            const Stopwatch interfaceWatch;
            const InterfaceRequests interfaceRequests =
                drawInterface(game, atlasTexture, windowViewport, context.inventoryOpen);
            profile.interfaceSeconds = interfaceWatch.elapsedSeconds();
            if (interfaceRequests.toggleInventory)
            {
                context.inventoryOpen = !context.inventoryOpen;
                context.inventoryToggled = true;
            }
            if (interfaceRequests.useInventorySlot.has_value())
            {
                game.useInventoryItem(*interfaceRequests.useInventorySlot);
            }
            const std::optional<glm::vec2> internalCursor =
                windowToInternal(reading.cursor, reading.size, reading.framebufferSize);
            const bool mouseAvailable = internalCursor.has_value() &&
                                        !ImGui::GetIO().WantCaptureMouse && !context.inventoryOpen;
            if (!mouseAvailable)
            {
                context.input.clearButton(InputButton::PrimaryAttack);
            }

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
            renderer.render(scene, reading.framebufferSize.x, reading.framebufferSize.y);
            profile.renderSeconds = renderWatch.elapsedSeconds();
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
            window.present();
        }

        return EXIT_SUCCESS;
    }
}
