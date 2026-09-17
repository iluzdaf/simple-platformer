#include "debug_overlay_ui.hpp"

#include "debug_overlay.hpp"
#include "graphics/display_viewport.hpp"

#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>

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

    ImVec2 screenPosition(
        glm::vec2 worldPosition,
        const simple_platformer::DebugOverlay& scene,
        const simple_platformer::WindowViewport& viewport)
    {
        return {
            viewport.topLeft.x +
                (worldPosition.x - scene.cameraBounds.position.x) * viewport.scale.x,
            viewport.topLeft.y +
                (worldPosition.y - scene.cameraBounds.position.y) * viewport.scale.y};
    }

    void drawWorldBounds(
        ImDrawList& drawList,
        const simple_platformer::Aabb& bounds,
        const simple_platformer::DebugOverlay& scene,
        const simple_platformer::WindowViewport& viewport,
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
        const simple_platformer::DebugOverlay& scene,
        const simple_platformer::WindowViewport& viewport)
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

    void drawActorSensor(
        ImDrawList& drawList,
        const simple_platformer::ActorDebugInfo& actor,
        const simple_platformer::DebugOverlay& scene,
        const simple_platformer::WindowViewport& viewport)
    {
        if (!actor.sensor.has_value())
        {
            return;
        }

        const simple_platformer::SensorDebugInfo& sensor = actor.sensor.value();
        const ImVec2 observer = screenPosition(sensor.observerCenter, scene, viewport);
        drawList.AddCircle(
            observer,
            sensor.noticeDistance * viewport.scale.x,
            IM_COL32(160, 96, 255, 110),
            48,
            1.0F);

        if (sensor.visibleTargetCenter.has_value())
        {
            drawList.AddLine(
                observer,
                screenPosition(sensor.visibleTargetCenter.value(), scene, viewport),
                IM_COL32(80, 255, 96, 230),
                2.0F);
        }

        if (sensor.rememberedTargetFeet.has_value())
        {
            constexpr float MarkerRadius = 4.0F;
            const ImU32 memoryColour = IM_COL32(255, 224, 64, 240);
            const ImVec2 remembered =
                screenPosition(sensor.rememberedTargetFeet.value(), scene, viewport);
            drawList.AddLine(observer, remembered, memoryColour, 1.5F);
            drawList.AddLine(
                {remembered.x - MarkerRadius, remembered.y - MarkerRadius},
                {remembered.x + MarkerRadius, remembered.y + MarkerRadius},
                memoryColour,
                2.0F);
            drawList.AddLine(
                {remembered.x - MarkerRadius, remembered.y + MarkerRadius},
                {remembered.x + MarkerRadius, remembered.y - MarkerRadius},
                memoryColour,
                2.0F);
            char memoryLabel[32]{};
            std::snprintf(memoryLabel, sizeof(memoryLabel), "%.2fs", sensor.memoryRemaining);
            drawList.AddText(
                {remembered.x + MarkerRadius + 2.0F, remembered.y - MarkerRadius},
                memoryColour,
                memoryLabel);
        }
    }

    void drawActorWorldLabel(
        ImDrawList& drawList,
        const simple_platformer::ActorDebugInfo& actor,
        const simple_platformer::DebugOverlay& scene,
        const simple_platformer::WindowViewport& viewport)
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

    void drawProjectile(
        ImDrawList& drawList,
        const simple_platformer::ProjectileDebugInfo& projectile,
        const simple_platformer::DebugOverlay& scene,
        const simple_platformer::WindowViewport& viewport)
    {
        const ImU32 colour = IM_COL32(255, 160, 64, 255);
        drawWorldBounds(drawList, projectile.bounds, scene, viewport, colour);

        ImVec2 labelPosition = screenPosition(projectile.bounds.position, scene, viewport);
        labelPosition.y += projectile.bounds.size.y * viewport.scale.y + 2.0F;
        char label[64]{};
        if (projectile.owner.has_value())
        {
            std::snprintf(
                label,
                sizeof(label),
                "%u\n%.2f",
                projectile.owner->value,
                projectile.remainingLifetime);
        }
        else
        {
            std::snprintf(label, sizeof(label), "none\n%.2f", projectile.remainingLifetime);
        }
        drawList.AddText(labelPosition, colour, label);
    }

    void drawTextLine(
        ImDrawList& drawList,
        ImVec2& position,
        const char* text,
        ImU32 colour,
        float indentation = 0.0F)
    {
        drawList.AddText({position.x + indentation, position.y}, colour, text);
        position.y += ImGui::GetTextLineHeight();
    }

    void drawActorText(
        ImDrawList& drawList,
        const simple_platformer::ActorDebugInfo& actor,
        ImVec2& position)
    {
        constexpr float Indentation = 12.0F;
        constexpr float ActorGap = 4.0F;
        constexpr ImU32 HeadingColour = IM_COL32(255, 255, 255, 255);
        constexpr ImU32 DetailColour = IM_COL32(224, 224, 224, 255);

        const std::string label = labelFor(actor);
        drawTextLine(drawList, position, label.c_str(), HeadingColour);

        char text[96]{};
        std::snprintf(
            text,
            sizeof(text),
            "pos:    %.1f, %.1f",
            actor.collider.position.x,
            actor.collider.position.y);
        drawTextLine(drawList, position, text, DetailColour, Indentation);

        if (actor.pathFollower.has_value())
        {
            const simple_platformer::PathFollowerDebugInfo& follower = actor.pathFollower.value();
            if (follower.hasPath)
            {
                std::snprintf(
                    text, sizeof(text), "path:   %zu / %zu", follower.nextStep, follower.stepCount);
            }
            else
            {
                std::snprintf(text, sizeof(text), "path:   %d / %d", 0, 0);
            }
            drawTextLine(drawList, position, text, DetailColour, Indentation);

            if (follower.destination.has_value())
            {
                std::snprintf(
                    text,
                    sizeof(text),
                    "dest:   %d, %d",
                    follower.destination->x,
                    follower.destination->y);
            }
            else
            {
                std::snprintf(text, sizeof(text), "dest:   none");
            }
            drawTextLine(drawList, position, text, DetailColour, Indentation);

            std::snprintf(text, sizeof(text), "repath: %.2f", follower.repathRemaining);
            drawTextLine(drawList, position, text, DetailColour, Indentation);
        }

        if (actor.sprite.has_value())
        {
            std::snprintf(
                text,
                sizeof(text),
                "frame:  %zu (%.0f, %.0f)",
                actor.sprite->atlasFrame,
                actor.sprite->atlasPosition.x,
                actor.sprite->atlasPosition.y);
            drawTextLine(drawList, position, text, DetailColour, Indentation);
        }
        position.y += ActorGap;
    }
}

namespace simple_platformer
{
    void drawDebugOverlay(const DebugOverlay& scene, const std::optional<WindowViewport>& viewport)
    {
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

        constexpr float ActorTextWidth = 180.0F;
        constexpr float ActorTextMargin = 8.0F;
        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImVec2 actorTextPosition = {
            mainViewport->WorkPos.x + mainViewport->WorkSize.x - ActorTextWidth - ActorTextMargin,
            mainViewport->WorkPos.y + ActorTextMargin};
        for (const ActorDebugInfo& actor : scene.actors)
        {
            drawActorText(*drawList, actor, actorTextPosition);
            if (!viewport.has_value())
            {
                continue;
            }

            drawActorSensor(*drawList, actor, scene, *viewport);
            drawActorPath(*drawList, actor, scene, *viewport);

            if (actor.biteHitbox.has_value())
            {
                drawWorldBounds(
                    *drawList,
                    actor.biteHitbox.value(),
                    scene,
                    *viewport,
                    IM_COL32(255, 64, 224, 255));
            }

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

        if (viewport.has_value())
        {
            for (const ProjectileDebugInfo& projectile : scene.projectiles)
            {
                drawProjectile(*drawList, projectile, scene, *viewport);
            }
            for (const PickupDebugInfo& pickup : scene.pickups)
            {
                const ImU32 colour = IM_COL32(96, 255, 160, 255);
                drawWorldBounds(*drawList, pickup.bounds, scene, *viewport, colour);
                drawList->AddText(
                    screenPosition(pickup.bounds.position, scene, *viewport),
                    colour,
                    pickup.itemName.c_str());
            }
        }
    }
}
