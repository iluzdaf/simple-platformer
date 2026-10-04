#include "application.hpp"

#include <cstddef>
#include <cstdlib>
#include <optional>
#include <utility>

#include <glm/vec2.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>

#include "content/game_catalogs.hpp"
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
            // Which NPC body's navigation the overlay shows; N moves to the next.
            std::size_t debugBodyIndex = 0;
            // B with the overlay open breaks the tile under the cursor, as a shot would.
            bool breakTileRequested = false;
            bool inventoryOpen = false;
            // Set when the inventory opens or closes, or the game restarts.
            // The next frame clears gameplay input and discards
            // accumulated time before simulation can continue.
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

        void restartIfRequested(ApplicationContext& context, Game& game)
        {
            if (!context.restartRequested)
            {
                return;
            }
            if (game.complete())
            {
                game.restart();
                context.inventoryOpen = false;
                context.aimDirection = {1.0F, 0.0F};
                context.playInterrupted = true;
            }
            context.restartRequested = false;
        }

        void applyInterfaceRequests(
            ApplicationContext& context,
            Game& game,
            const InterfaceRequests& interfaceRequests)
        {
            if (interfaceRequests.toggleInventory)
            {
                context.inventoryOpen = !context.inventoryOpen;
                context.playInterrupted = true;
            }
            if (interfaceRequests.useInventorySlot.has_value())
            {
                game.useInventoryItem(*interfaceRequests.useInventorySlot);
            }
        }

        void breakTileIfRequested(
            ApplicationContext& context,
            Game& game,
            const std::optional<glm::vec2>& internalCursor)
        {
            if (!context.breakTileRequested)
            {
                return;
            }
            if (internalCursor.has_value())
            {
                game.breakTileAt(*internalCursor);
            }
            context.breakTileRequested = false;
        }

        // UI mouse capture changes gameplay aim and firing, not simulation time.
        std::optional<glm::vec2> gameplayCursor(
            const ApplicationContext& context,
            const std::optional<glm::vec2>& internalCursor,
            bool mouseCaptured)
        {
            if (mouseCaptured || context.inventoryOpen)
            {
                return std::nullopt;
            }
            return internalCursor;
        }

        // Clear buttons and pending edges when gameplay cannot use them. Keyboard
        // capture blocks input, but does not pause simulation or reset accumulated time.
        void preparePlayerInput(
            ApplicationContext& context,
            bool simulationBlocked,
            const std::optional<glm::vec2>& gameCursor,
            bool keyboardCaptured)
        {
            if (simulationBlocked || context.playInterrupted || keyboardCaptured)
            {
                context.input = {};
            }
            else if (!gameCursor.has_value())
            {
                context.input.clearButton(InputButton::PrimaryAttack);
            }
        }

        // Consumes held buttons and pending edges for one simulation step. A gameplay
        // cursor updates aim; without one, keep the last aim and suppress firing.
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

        void runGameUpdates(
            ApplicationContext& context,
            Game& game,
            FixedStep& fixedStep,
            float frameSeconds,
            bool simulationBlocked,
            const std::optional<glm::vec2>& gameCursor)
        {
            if (simulationBlocked || context.playInterrupted)
            {
                // Time that built up would otherwise be simulated in a burst on resuming.
                fixedStep.reset();
                context.playInterrupted = false;
                return;
            }

            fixedStep.advance(
                frameSeconds,
                [&](float deltaTime)
                {
                    const InputIntentions intentions = playerIntentions(context, game, gameCursor);
                    game.update(intentions, deltaTime);
                });
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
        const int atlas = renderer.loadTexture("assets/textures/sprites.png");
        const Texture atlasTexture = renderer.texture(atlas);
        FixedStep fixedStep;
        LevelCatalog levelCatalog = loadLevelCatalog("assets/levels/levels.json");
        GameCatalogs gameCatalogs =
            loadGameCatalogs("assets/catalogs", {atlasTexture.width, atlasTexture.height});
        Game game(
            atlas,
            std::move(levelCatalog),
            std::move(gameCatalogs),
            static_cast<float>(fixedStep.stepSeconds()));
        Stopwatch frameClock;

        while (!window.shouldClose())
        {
            glfwPollEvents();
            imgui.beginFrame();

            restartIfRequested(context, game);

            const WindowReading reading = window.read();
            const std::optional<WindowViewport> windowViewport =
                makeWindowViewport(reading.size, reading.framebufferSize);

            const float frameSeconds = frameClock.lapSeconds();
            const InterfaceRequests interfaceRequests =
                drawInterface(game, atlasTexture, windowViewport, context.inventoryOpen);

            applyInterfaceRequests(context, game, interfaceRequests);

            const std::optional<glm::vec2> internalCursor =
                windowToInternal(reading.cursor, reading.size, reading.framebufferSize);
            breakTileIfRequested(context, game, internalCursor);

            const std::optional<glm::vec2> gameCursor =
                gameplayCursor(context, internalCursor, ImGui::GetIO().WantCaptureMouse);

            const bool simulationBlocked = context.inventoryOpen || game.complete();
            preparePlayerInput(
                context, simulationBlocked, gameCursor, ImGui::GetIO().WantCaptureKeyboard);

            runGameUpdates(context, game, fixedStep, frameSeconds, simulationBlocked, gameCursor);

            const RenderScene scene = game.buildScene();

            renderer.render(scene, reading.framebufferSize.x, reading.framebufferSize.y);

            if (context.showDebugOverlay)
            {
                drawDebugTools(
                    game.debugOverlay(
                        static_cast<float>(atlasTexture.width),
                        internalCursor,
                        context.debugBodyIndex),
                    windowViewport);
            }
            imgui.render();
            window.present();
        }

        return EXIT_SUCCESS;
    }
}
