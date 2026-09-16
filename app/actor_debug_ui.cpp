#include "actor_debug_ui.hpp"

#include "actor_debug.hpp"
#include "graphics/display_viewport.hpp"

#include <cstddef>
#include <optional>
#include <string>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/render/animation.hpp"

namespace
{
    const char* nameOf(simple_platformer::AnimationName animation)
    {
        using simple_platformer::AnimationName;

        switch (animation)
        {
        case AnimationName::Idle:
            return "Idle";
        case AnimationName::Move:
            return "Move";
        case AnimationName::Jump:
            return "Jump";
        case AnimationName::Fall:
            return "Fall";
        case AnimationName::Attack:
            return "Attack";
        case AnimationName::Death:
            return "Death";
        }

        return "Unknown";
    }

    const char* nameOf(simple_platformer::NpcState state)
    {
        using simple_platformer::NpcState;

        switch (state)
        {
        case NpcState::Idle:
            return "Idle";
        case NpcState::Patrol:
            return "Patrol";
        case NpcState::Chase:
            return "Chase";
        case NpcState::Bite:
            return "Bite";
        }

        return "Unknown";
    }

    const char* nameOf(simple_platformer::Traversal traversal)
    {
        using simple_platformer::Traversal;

        switch (traversal)
        {
        case Traversal::Fly:
            return "Fly";
        case Traversal::Walk:
            return "Walk";
        case Traversal::Fall:
            return "Fall";
        case Traversal::Jump:
            return "Jump";
        }

        return "Unknown";
    }

    std::string labelFor(const simple_platformer::ActorDebugInfo& actor)
    {
        switch (actor.kind)
        {
        case simple_platformer::ActorDebugKind::Player:
            return "Player";
        case simple_platformer::ActorDebugKind::Npc:
            return "NPC " + std::to_string(actor.id.value);
        case simple_platformer::ActorDebugKind::Actor:
            return "Actor " + std::to_string(actor.id.value);
        }

        return "Actor";
    }

    struct GameViewport
    {
        ImVec2 position;
        ImVec2 scale;
    };

    std::optional<GameViewport> gameViewport(
        GLFWwindow* window,
        int framebufferWidth,
        int framebufferHeight)
    {
        int windowWidth = 0;
        int windowHeight = 0;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        if (windowWidth <= 0 || windowHeight <= 0)
        {
            return std::nullopt;
        }

        const std::optional<simple_platformer::DisplayViewport> displayViewport =
            simple_platformer::makeDisplayViewport({framebufferWidth, framebufferHeight});
        if (!displayViewport.has_value())
        {
            return std::nullopt;
        }
        const float framebufferToWindowX =
            static_cast<float>(windowWidth) / static_cast<float>(framebufferWidth);
        const float framebufferToWindowY =
            static_cast<float>(windowHeight) / static_cast<float>(framebufferHeight);
        return GameViewport{
            {displayViewport->topLeftMargin.x * framebufferToWindowX,
             displayViewport->topLeftMargin.y * framebufferToWindowY},
            {displayViewport->scale * framebufferToWindowX,
             displayViewport->scale * framebufferToWindowY}};
    }

    ImVec2 screenPosition(
        glm::vec2 worldPosition,
        const simple_platformer::ActorDebugScene& scene,
        const GameViewport& viewport)
    {
        return {
            viewport.position.x +
                (worldPosition.x - scene.cameraBounds.position.x) * viewport.scale.x,
            viewport.position.y +
                (worldPosition.y - scene.cameraBounds.position.y) * viewport.scale.y};
    }

    void drawWorldBounds(
        ImDrawList& drawList,
        const simple_platformer::Aabb& bounds,
        const simple_platformer::ActorDebugScene& scene,
        const GameViewport& viewport,
        ImU32 colour)
    {
        const ImVec2 minimum = screenPosition(bounds.position, scene, viewport);
        const ImVec2 maximum = {
            minimum.x + bounds.size.x * viewport.scale.x,
            minimum.y + bounds.size.y * viewport.scale.y};
        drawList.AddRect(minimum, maximum, colour, 0.0F, 0, 2.0F);
    }

    ImU32 pathColour(const simple_platformer::PathConnectionDebugInfo& connection)
    {
        if (connection.completed)
        {
            return IM_COL32(128, 128, 128, 180);
        }

        using simple_platformer::Traversal;
        switch (connection.traversal)
        {
        case Traversal::Fly:
            return IM_COL32(64, 224, 255, 255);
        case Traversal::Walk:
            return IM_COL32(80, 224, 96, 255);
        case Traversal::Fall:
            return IM_COL32(255, 160, 64, 255);
        case Traversal::Jump:
            return IM_COL32(224, 80, 255, 255);
        }

        return IM_COL32(255, 255, 255, 255);
    }

    void drawActorPath(
        ImDrawList& drawList,
        const simple_platformer::ActorDebugInfo& actor,
        const simple_platformer::ActorDebugScene& scene,
        const GameViewport& viewport)
    {
        if (!actor.pathFollower.has_value())
        {
            return;
        }

        const simple_platformer::PathFollowerDebugInfo& follower = actor.pathFollower.value();
        for (const simple_platformer::PathConnectionDebugInfo& connection : follower.connections)
        {
            const ImVec2 from = screenPosition(connection.fromFeet, scene, viewport);
            const ImVec2 to = screenPosition(connection.toFeet, scene, viewport);
            const ImU32 colour = pathColour(connection);
            const float thickness = connection.next ? 3.0F : 2.0F;
            if (connection.sampledFeet.size() >= 2)
            {
                for (std::size_t index = 1; index < connection.sampledFeet.size(); ++index)
                {
                    drawList.AddLine(
                        screenPosition(connection.sampledFeet[index - 1], scene, viewport),
                        screenPosition(connection.sampledFeet[index], scene, viewport),
                        colour,
                        thickness);
                }
            }
            else
            {
                drawList.AddLine(from, to, colour, thickness);
            }
            drawList.AddCircleFilled(to, connection.next ? 4.0F : 3.0F, colour);

            if (connection.next)
            {
                const glm::vec2 labelWorldPosition =
                    connection.sampledFeet.empty()
                        ? (connection.fromFeet + connection.toFeet) * 0.5F
                        : connection.sampledFeet[connection.sampledFeet.size() / 2];
                const ImVec2 labelPosition = screenPosition(labelWorldPosition, scene, viewport);
                const char* traversalName = nameOf(connection.traversal);
                drawList.AddText(
                    {labelPosition.x + 1.0F, labelPosition.y + 1.0F},
                    IM_COL32(0, 0, 0, 220),
                    traversalName);
                drawList.AddText(labelPosition, colour, traversalName);
                drawList.AddLine(
                    screenPosition(simple_platformer::feetOf(actor.collider), scene, viewport),
                    to,
                    IM_COL32(255, 255, 255, 220));
            }
        }
    }

    void drawActorWorldLabel(
        ImDrawList& drawList,
        const simple_platformer::ActorDebugInfo& actor,
        const simple_platformer::ActorDebugScene& scene,
        const GameViewport& viewport)
    {
        const glm::vec2 labelWorldPosition =
            actor.sprite.has_value() ? actor.sprite->bounds.position : actor.collider.position;
        ImVec2 labelPosition = screenPosition(labelWorldPosition, scene, viewport);
        const float lineHeight = ImGui::GetTextLineHeight();
        const std::string actorLabel = labelFor(actor);
        drawList.AddText(labelPosition, IM_COL32(255, 255, 255, 255), actorLabel.c_str());

        if (actor.animation.has_value())
        {
            labelPosition.y += lineHeight;
            drawList.AddText(
                labelPosition, IM_COL32(255, 255, 255, 255), nameOf(actor.animation.value()));
        }
        if (actor.npcState.has_value())
        {
            labelPosition.y += lineHeight;
            drawList.AddText(
                labelPosition, IM_COL32(255, 255, 255, 255), nameOf(actor.npcState.value()));
        }
    }

    void drawActorWindow(const simple_platformer::ActorDebugInfo& actor, std::size_t index)
    {
        constexpr float WindowWidth = 180.0F;
        constexpr float WindowHeight = 120.0F;
        constexpr float WindowMargin = 8.0F;
        constexpr float WindowGap = 8.0F;
        constexpr ImGuiWindowFlags Flags =
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        const std::string label = labelFor(actor);
        const std::string windowName =
            label + " debug###actor-debug-" + std::to_string(actor.id.value);
        const float windowX = WindowMargin + static_cast<float>(index) * (WindowWidth + WindowGap);

        ImGui::SetNextWindowPos({windowX, WindowMargin}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({WindowWidth, WindowHeight}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.3F);
        if (ImGui::Begin(windowName.c_str(), nullptr, Flags))
        {
            ImGui::Text("pos: %.1f, %.1f", actor.collider.position.x, actor.collider.position.y);
            if (actor.pathFollower.has_value())
            {
                const simple_platformer::PathFollowerDebugInfo& follower =
                    actor.pathFollower.value();
                if (follower.hasPath)
                {
                    ImGui::Text("path: %zu / %zu", follower.nextStep, follower.stepCount);
                }
                else
                {
                    ImGui::Text("path: none");
                }
                if (follower.destination.has_value())
                {
                    ImGui::Text(
                        "destination: %d, %d", follower.destination->x, follower.destination->y);
                }
                ImGui::Text("repath: %.2f", follower.repathRemaining);
            }
            if (actor.sprite.has_value())
            {
                ImGui::Text(
                    "frame: %zu (%.0f, %.0f)",
                    actor.sprite->atlasFrame,
                    actor.sprite->atlasPosition.x,
                    actor.sprite->atlasPosition.y);
            }
        }
        ImGui::End();
    }
}

namespace simple_platformer
{
    void drawActorDebugUi(
        const ActorDebugScene& scene,
        GLFWwindow* window,
        int framebufferWidth,
        int framebufferHeight)
    {
        const std::optional<GameViewport> viewport =
            gameViewport(window, framebufferWidth, framebufferHeight);
        ImDrawList* drawList = ImGui::GetBackgroundDrawList();

        if (viewport.has_value())
        {
            drawWorldBounds(
                *drawList, scene.cameraBounds, scene, *viewport, IM_COL32(64, 224, 255, 255));
            drawWorldBounds(
                *drawList, scene.cameraDeadZone, scene, *viewport, IM_COL32(255, 224, 64, 255));
            drawList->AddText(
                screenPosition(scene.cameraDeadZone.position, scene, *viewport),
                IM_COL32(255, 224, 64, 255),
                "camera dead zone");
        }

        for (std::size_t index = 0; index < scene.actors.size(); ++index)
        {
            const ActorDebugInfo& actor = scene.actors[index];
            drawActorWindow(actor, index);
            if (!viewport.has_value())
            {
                continue;
            }

            drawActorPath(*drawList, actor, scene, *viewport);

            if (actor.sprite.has_value())
            {
                drawWorldBounds(
                    *drawList,
                    actor.sprite->bounds,
                    scene,
                    *viewport,
                    IM_COL32(255, 255, 255, 255));
            }
            drawActorWorldLabel(*drawList, actor, scene, *viewport);
            drawWorldBounds(
                *drawList, actor.collider, scene, *viewport, IM_COL32(255, 64, 64, 255));
        }
    }
}
