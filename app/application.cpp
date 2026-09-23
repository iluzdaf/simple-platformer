#include "application.hpp"

#include <cstdlib>
#include <optional>

#include <glm/vec2.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>

#include "content/level_catalog.hpp"
#include "debug/debug_overlay_ui.hpp"
#include "debug/frame_profile_ui.hpp"
#include "debug/frame_selection.hpp"
#include "game/game.hpp"
#include "graphics/display_viewport.hpp"
#include "graphics/game_window.hpp"
#include "graphics/imgui_session.hpp"
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
        struct ApplicationContext
        {
            InputState input;
            glm::vec2 aimDirection = {1.0F, 0.0F};
            bool showDebugOverlay = false;
            bool inventoryOpen = false;
            // Set when the inventory opens or closes or the game restarts, so the next
            // step discards the time and input edges that built up across the change.
            bool playInterrupted = false;
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
                context->playInterrupted = true;
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
                return;
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

        // What the player asks of one simulation step: the buttons pressed since the last
        // one, aimed at the cursor while the game has it. Without the cursor the last aim
        // holds and no shot fires.
        InputIntentions playerIntentions(
            ApplicationContext& context,
            const Game& game,
            const std::optional<glm::vec2>& gameCursor)
        {
            InputIntentions intentions = context.input.consumeIntentions();
            if (gameCursor.has_value())
            {
                const glm::vec2 aimDirection = game.playerAimDirection(*gameCursor);
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
            return intentions;
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
        FrameSelection frameSelection;
        Stopwatch frameClock;

        while (!window.shouldClose())
        {
            glfwPollEvents();
            imgui.beginFrame();

            if (context.restartRequested)
            {
                if (game.complete())
                {
                    game.restart();
                    context.inventoryOpen = false;
                    context.aimDirection = {1.0F, 0.0F};
                    context.playInterrupted = true;
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
                context.playInterrupted = true;
            }
            if (interfaceRequests.useInventorySlot.has_value())
            {
                game.useInventoryItem(*interfaceRequests.useInventorySlot);
            }
            // The game has the cursor while it is over the image and no UI wants the mouse.
            std::optional<glm::vec2> gameCursor =
                windowToInternal(reading.cursor, reading.size, reading.framebufferSize);
            if (ImGui::GetIO().WantCaptureMouse || context.inventoryOpen)
            {
                gameCursor.reset();
            }

            // What input survives into the step: nothing while paused, across an
            // interruption, or while ImGui has the keyboard; no attack without the cursor.
            const bool paused = context.inventoryOpen || game.complete();
            if (paused || context.playInterrupted || ImGui::GetIO().WantCaptureKeyboard)
            {
                context.input = {};
            }
            else if (!gameCursor.has_value())
            {
                context.input.clearButton(InputButton::PrimaryAttack);
            }

            if (paused || context.playInterrupted)
            {
                // Time that built up would otherwise be simulated in a burst on resuming.
                fixedStep.reset();
                context.playInterrupted = false;
            }
            else
            {
                const auto step = [&](float deltaTime)
                {
                    const InputIntentions intentions = playerIntentions(context, game, gameCursor);
                    game.update(intentions, deltaTime, &profile);
                };
                const Stopwatch simulationWatch;
                const FixedStepResult stepped = fixedStep.advance(profile.frameSeconds, step);
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
                drawFrameProfile(frameHistory, frameSelection);
            }
            imgui.render();
            window.present();
        }

        return EXIT_SUCCESS;
    }
}
