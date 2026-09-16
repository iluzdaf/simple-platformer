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

    void drawActorWindow(const simple_platformer::ActorDebugInfo& actor, std::size_t index)
    {
        constexpr ImGuiWindowFlags Flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;
        const std::string label = labelFor(actor);
        const std::string windowName =
            label + " debug###actor-debug-" + std::to_string(actor.id.value);
        const float windowX = 8.0F + static_cast<float>(index) * 220.0F;

        ImGui::SetNextWindowPos({windowX, 8.0F}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.75F);
        if (ImGui::Begin(windowName.c_str(), nullptr, Flags))
        {
            ImGui::Text("%s", label.c_str());
            ImGui::Text("pos: %.1f, %.1f", actor.collider.position.x, actor.collider.position.y);
            if (actor.animation.has_value())
            {
                ImGui::Text("animation: %s", nameOf(*actor.animation));
            }
            if (actor.npcState.has_value())
            {
                ImGui::Text("state: %s", nameOf(*actor.npcState));
            }
            if (actor.sprite.has_value())
            {
                ImGui::Text(
                    "frame: %zu (%.0f, %.0f)",
                    actor.sprite->atlasFrame,
                    actor.sprite->atlasPosition.x,
                    actor.sprite->atlasPosition.y);
                ImGui::Text(
                    "sprite: %.1f, %.1f  %.0f x %.0f",
                    actor.sprite->bounds.position.x,
                    actor.sprite->bounds.position.y,
                    actor.sprite->bounds.size.x,
                    actor.sprite->bounds.size.y);
            }
            ImGui::TextColored(
                {1.0F, 0.25F, 0.25F, 1.0F},
                "collider: %.1f, %.1f  %.0f x %.0f",
                actor.collider.position.x,
                actor.collider.position.y,
                actor.collider.size.x,
                actor.collider.size.y);
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

            if (actor.sprite.has_value())
            {
                drawWorldBounds(
                    *drawList,
                    actor.sprite->bounds,
                    scene,
                    *viewport,
                    IM_COL32(255, 255, 255, 255));
                const ImVec2 labelPosition =
                    screenPosition(actor.sprite->bounds.position, scene, *viewport);
                drawList->AddText(
                    labelPosition, IM_COL32(255, 255, 255, 255), labelFor(actor).c_str());
            }
            drawWorldBounds(
                *drawList, actor.collider, scene, *viewport, IM_COL32(255, 64, 64, 255));
        }
    }
}
