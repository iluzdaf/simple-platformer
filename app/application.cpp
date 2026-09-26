#include "application.hpp"

#include <cstddef>
#include <cstdlib>
#include <optional>

#include <glm/vec2.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>

#include "content/level_catalog.hpp"
#include "debug/debug_tools.hpp"
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
            DebugToolVisibility debugToolVisibility;
            // Which NPC body's navigation the overlay shows; N moves to the next.
            std::size_t debugBodyIndex = 0;
            // B with the overlay open breaks the tile under the cursor, as a shot would.
            bool breakTileRequested = false;
            bool inventoryOpen = false;
            // P pauses the simulation; . runs one fixed step while paused.
            bool simulationPaused = false;
            bool stepRequested = false;
            // Set when the inventory opens or closes, the simulation pauses or resumes,
            // or the game restarts, so the next step discards the time and input edges
            // that built up across the change.
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
            if (key == GLFW_KEY_P && action == GLFW_PRESS)
            {
                context->simulationPaused = !context->simulationPaused;
                context->playInterrupted = true;
                return;
            }
            if (key == GLFW_KEY_PERIOD && action == GLFW_PRESS && context->simulationPaused)
            {
                context->stepRequested = true;
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
            if (key == GLFW_KEY_1 && action == GLFW_PRESS && context->showDebugOverlay)
            {
                context->debugToolVisibility.frameProfileDetails =
                    !context->debugToolVisibility.frameProfileDetails;
                return;
            }
            if (key == GLFW_KEY_2 && action == GLFW_PRESS && context->showDebugOverlay)
            {
                context->debugToolVisibility.worldAndCameraOverlay =
                    !context->debugToolVisibility.worldAndCameraOverlay;
                return;
            }
            if (key == GLFW_KEY_3 && action == GLFW_PRESS && context->showDebugOverlay)
            {
                context->debugToolVisibility.actorText = !context->debugToolVisibility.actorText;
                return;
            }
            if (key == GLFW_KEY_4 && action == GLFW_PRESS && context->showDebugOverlay)
            {
                context->debugToolVisibility.navigationCacheText =
                    !context->debugToolVisibility.navigationCacheText;
                return;
            }
            if (key == GLFW_KEY_5 && action == GLFW_PRESS && context->showDebugOverlay)
            {
                context->debugToolVisibility.stateMachine =
                    !context->debugToolVisibility.stateMachine;
                return;
            }
            if (key == GLFW_KEY_N && action == GLFW_PRESS)
            {
                ++context->debugBodyIndex;
                return;
            }
            if (key == GLFW_KEY_B && action == GLFW_PRESS && context->showDebugOverlay)
            {
                context->breakTileRequested = true;
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
        FixedStep fixedStep;
        Game game(
            atlas,
            loadLevelCatalog("assets/levels.json"),
            static_cast<float>(fixedStep.stepSeconds()));
        DebugTools debugTools;
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
            const InterfaceRequests interfaceRequests = drawInterface(
                game,
                atlasTexture,
                windowViewport,
                context.inventoryOpen,
                context.simulationPaused);
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
            const std::optional<glm::vec2> internalCursor =
                windowToInternal(reading.cursor, reading.size, reading.framebufferSize);
            if (context.breakTileRequested)
            {
                if (internalCursor.has_value())
                {
                    game.breakTileAt(*internalCursor);
                }
                context.breakTileRequested = false;
            }
            if (context.showDebugOverlay && context.debugToolVisibility.stateMachine &&
                internalCursor.has_value() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                !ImGui::GetIO().WantCaptureMouse)
            {
                const std::optional<ActorId> clicked = game.machineActorAt(*internalCursor);
                if (clicked.has_value())
                {
                    if (debugTools.machineActor == clicked)
                    {
                        debugTools.machineActor.reset();
                    }
                    else
                    {
                        debugTools.machineActor = clicked;
                    }
                    context.input.clearButton(InputButton::PrimaryAttack);
                }
            }
            // The game has the cursor while it is over the image and no UI wants the mouse.
            std::optional<glm::vec2> gameCursor = internalCursor;
            if (ImGui::GetIO().WantCaptureMouse || context.inventoryOpen)
            {
                gameCursor.reset();
            }

            // What input survives into the step: nothing while paused, across an
            // interruption, or while ImGui has the keyboard; no attack without the cursor.
            const bool paused =
                context.inventoryOpen || game.complete() || context.simulationPaused;
            if (paused || context.playInterrupted || ImGui::GetIO().WantCaptureKeyboard)
            {
                context.input = {};
            }
            else if (!gameCursor.has_value())
            {
                context.input.clearButton(InputButton::PrimaryAttack);
            }

            const auto step = [&](float deltaTime)
            {
                const InputIntentions intentions = playerIntentions(context, game, gameCursor);
                game.update(intentions, deltaTime, &profile);
            };
            if (paused || context.playInterrupted)
            {
                // Time that built up would otherwise be simulated in a burst on resuming.
                fixedStep.reset();
                context.playInterrupted = false;
                if (context.stepRequested && !context.inventoryOpen && !game.complete())
                {
                    const Stopwatch simulationWatch;
                    step(static_cast<float>(fixedStep.stepSeconds()));
                    profile.simulationTicks = 1;
                    profile.simulationSeconds = simulationWatch.elapsedSeconds();
                }
            }
            else
            {
                const Stopwatch simulationWatch;
                const FixedStepResult stepped = fixedStep.advance(profile.frameSeconds, step);
                profile.simulationTicks = static_cast<int>(stepped.updates);
                profile.simulationSeconds = simulationWatch.elapsedSeconds();
            }
            context.stepRequested = false;

            const Stopwatch sceneWatch;
            const RenderScene scene = game.buildScene();
            profile.sceneSeconds = sceneWatch.elapsedSeconds();
            const Stopwatch renderWatch;
            renderer.render(scene, reading.framebufferSize.x, reading.framebufferSize.y);
            profile.renderSeconds = renderWatch.elapsedSeconds();

            if (context.showDebugOverlay)
            {
                drawDebugTools(
                    debugTools,
                    profile,
                    game.debugOverlay(
                        static_cast<float>(atlasTexture.width),
                        internalCursor,
                        context.debugBodyIndex,
                        debugTools.machineActor),
                    windowViewport,
                    context.debugToolVisibility);
            }
            imgui.render();
            window.present();
        }

        return EXIT_SUCCESS;
    }
}
