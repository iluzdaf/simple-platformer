#include "debug_overlay_ui.hpp"

#include "debug_overlay.hpp"
#include "navigation_debug.hpp"
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
#include "ui/hud_draw.hpp"

namespace simple_platformer
{
    namespace
    {
        // Debug overlay palette. Adjust these values to tune every overlay colour.
        constexpr ImU32 CompletedPathColour = IM_COL32(128, 128, 128, 180);
        constexpr ImU32 FlyingPathColour = IM_COL32(64, 224, 255, 255);
        constexpr ImU32 WalkingPathColour = IM_COL32(80, 224, 96, 255);
        constexpr ImU32 FallingPathColour = IM_COL32(255, 160, 64, 255);
        constexpr ImU32 JumpingPathColour = IM_COL32(224, 80, 255, 255);
        constexpr ImU32 UnknownPathColour = IM_COL32(255, 255, 255, 255);
        constexpr ImU32 PathDestinationColour = IM_COL32(255, 255, 255, 230);
        constexpr ImU32 NextPathGuideColour = IM_COL32(255, 255, 255, 220);
        constexpr ImU32 SensorRangeColour = IM_COL32(160, 96, 255, 110);
        constexpr ImU32 VisibleTargetColour = IM_COL32(80, 255, 96, 230);
        constexpr ImU32 RememberedTargetColour = IM_COL32(255, 224, 64, 240);
        constexpr ImU32 PatrolRouteColour = IM_COL32(255, 192, 64, 190);
        constexpr ImU32 PatrolPointColour = IM_COL32(255, 224, 128, 255);
        constexpr ImU32 ActivePatrolPointColour = IM_COL32(255, 255, 255, 255);
        constexpr ImU32 WorldLabelColour = IM_COL32(255, 255, 255, 255);
        constexpr ImU32 ProjectileColour = IM_COL32(255, 160, 64, 255);
        constexpr ImU32 TextHeadingColour = IM_COL32(255, 255, 255, 255);
        constexpr ImU32 TextDetailColour = IM_COL32(224, 224, 224, 255);
        constexpr ImU32 CameraBoundsColour = IM_COL32(64, 224, 255, 255);
        constexpr ImU32 CameraDeadZoneColour = IM_COL32(64, 64, 64, 255);
        constexpr ImU32 BiteHitboxColour = IM_COL32(255, 64, 224, 255);
        constexpr ImU32 SpriteBoundsColour = IM_COL32(64, 64, 64, 255);
        constexpr ImU32 ColliderBoundsColour = IM_COL32(255, 64, 64, 255);
        constexpr ImU32 PickupColour = IM_COL32(96, 255, 160, 255);
        // The connection cache's cells: kept with connections, kept with none, and missing,
        // which after a break means dropped and not yet simulated again.
        constexpr ImU32 NavigationKeptColour = IM_COL32(64, 160, 255, 90);
        constexpr ImU32 NavigationEmptyColour = IM_COL32(128, 128, 128, 70);
        constexpr ImU32 NavigationMissingColour = IM_COL32(255, 96, 32, 220);

        constexpr float ActorTextGap = 4.0F;

        const char* nameOf(AnimationName animation)
        {
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

        const char* nameOf(NpcState state)
        {
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

        const char* nameOf(Traversal traversal)
        {
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

        std::string labelFor(const ActorDebugInfo& actor)
        {
            switch (actor.kind)
            {
            case ActorDebugKind::Player:
                return "Player";
            case ActorDebugKind::Npc:
                return "NPC " + std::to_string(actor.id.value);
            case ActorDebugKind::Actor:
                return "Actor " + std::to_string(actor.id.value);
            }

            return "Actor";
        }

        ImVec2 screenPosition(
            glm::vec2 worldPosition,
            const DebugOverlay& scene,
            const WindowViewport& viewport)
        {
            return {
                viewport.topLeft.x +
                    (worldPosition.x - scene.cameraBounds.position.x) * viewport.scale.x,
                viewport.topLeft.y +
                    (worldPosition.y - scene.cameraBounds.position.y) * viewport.scale.y};
        }

        // A cell of the connection cache: a filled cell while its connections are kept,
        // with their count, and an outlined one while they are missing.
        void drawNavigationCell(
            ImDrawList& drawList,
            const NavigationCellDebugInfo& cell,
            const DebugOverlay& scene,
            const WindowViewport& viewport)
        {
            const ImVec2 minimum = screenPosition(cell.bounds.position, scene, viewport);
            const ImVec2 maximum = {
                minimum.x + cell.bounds.size.x * viewport.scale.x,
                minimum.y + cell.bounds.size.y * viewport.scale.y};
            if (!cell.connections.has_value())
            {
                drawList.AddRect(minimum, maximum, NavigationMissingColour, 0.0F, 0, 2.0F);
                return;
            }
            const ImU32 colour =
                *cell.connections == 0 ? NavigationEmptyColour : NavigationKeptColour;
            drawList.AddRectFilled(minimum, maximum, colour);
            if (*cell.connections > 0)
            {
                char count[8];
                std::snprintf(count, sizeof(count), "%zu", *cell.connections);
                drawList.AddText({minimum.x + 1.0F, minimum.y}, WorldLabelColour, count);
            }
        }

        void drawWorldBounds(
            ImDrawList& drawList,
            const Aabb& bounds,
            const DebugOverlay& scene,
            const WindowViewport& viewport,
            ImU32 colour)
        {
            const ImVec2 minimum = screenPosition(bounds.position, scene, viewport);
            const ImVec2 maximum = {
                minimum.x + bounds.size.x * viewport.scale.x,
                minimum.y + bounds.size.y * viewport.scale.y};
            drawList.AddRect(minimum, maximum, colour, 0.0F, 0, 2.0F);
        }

        ImU32 pathColour(const PathConnectionDebugInfo& connection)
        {
            if (connection.completed)
            {
                return CompletedPathColour;
            }

            switch (connection.traversal)
            {
            case Traversal::Fly:
                return FlyingPathColour;
            case Traversal::Walk:
                return WalkingPathColour;
            case Traversal::Fall:
                return FallingPathColour;
            case Traversal::Jump:
                return JumpingPathColour;
            }

            return UnknownPathColour;
        }

        void drawActorPath(
            ImDrawList& drawList,
            const ActorDebugInfo& actor,
            const DebugOverlay& scene,
            const WindowViewport& viewport)
        {
            if (!actor.pathFollower.has_value())
            {
                return;
            }

            const PathFollowerDebugInfo& follower = actor.pathFollower.value();
            for (std::size_t index = 0; index < follower.connections.size(); ++index)
            {
                const PathConnectionDebugInfo& connection = follower.connections[index];
                const ImVec2 from = screenPosition(connection.fromFeet, scene, viewport);
                const ImVec2 to = screenPosition(connection.toFeet, scene, viewport);
                const ImU32 colour = pathColour(connection);
                const float thickness = connection.next ? 3.0F : 2.0F;
                if (connection.sampledFeet.size() >= 2)
                {
                    for (std::size_t sampleIndex = 1; sampleIndex < connection.sampledFeet.size();
                         ++sampleIndex)
                    {
                        drawList.AddLine(
                            screenPosition(
                                connection.sampledFeet[sampleIndex - 1], scene, viewport),
                            screenPosition(connection.sampledFeet[sampleIndex], scene, viewport),
                            colour,
                            thickness);
                    }
                }
                else
                {
                    drawList.AddLine(from, to, colour, thickness);
                }
                drawList.AddCircleFilled(to, connection.next ? 4.0F : 3.0F, colour);
                char pointLabel[16]{};
                std::snprintf(pointLabel, sizeof(pointLabel), "%zu", index + 1);
                const ImVec2 pointLabelSize = ImGui::CalcTextSize(pointLabel);
                drawList.AddText(
                    {to.x - pointLabelSize.x * 0.5F,
                     to.y - pointLabelSize.y - (connection.next ? 6.0F : 5.0F)},
                    colour,
                    pointLabel);

                if (connection.next)
                {
                    const glm::vec2 labelWorldPosition =
                        connection.sampledFeet.empty()
                            ? (connection.fromFeet + connection.toFeet) * 0.5F
                            : connection.sampledFeet[connection.sampledFeet.size() / 2];
                    const ImVec2 labelPosition =
                        screenPosition(labelWorldPosition, scene, viewport);
                    const char* traversalName = nameOf(connection.traversal);
                    drawShadowedText(drawList, labelPosition, colour, traversalName);
                    drawList.AddLine(
                        screenPosition(feetOf(actor.collider), scene, viewport),
                        to,
                        NextPathGuideColour);
                }
            }

            if (follower.destinationFeet.has_value())
            {
                constexpr float DestinationRadius = 6.0F;
                const ImVec2 destination =
                    screenPosition(follower.destinationFeet.value(), scene, viewport);
                drawList.AddCircle(destination, DestinationRadius, PathDestinationColour, 16, 2.0F);
                drawList.AddText(
                    {destination.x + DestinationRadius + 2.0F,
                     destination.y - ImGui::GetTextLineHeight() * 0.5F},
                    PathDestinationColour,
                    "Dest");
            }
        }

        void drawActorSensor(
            ImDrawList& drawList,
            const ActorDebugInfo& actor,
            const DebugOverlay& scene,
            const WindowViewport& viewport)
        {
            if (!actor.sensor.has_value())
            {
                return;
            }

            const SensorDebugInfo& sensor = actor.sensor.value();
            const ImVec2 observer = screenPosition(sensor.observerCenter, scene, viewport);
            drawList.AddCircle(
                observer, sensor.noticeDistance * viewport.scale.x, SensorRangeColour, 48, 1.0F);

            if (sensor.visibleTargetCenter.has_value())
            {
                drawList.AddLine(
                    observer,
                    screenPosition(sensor.visibleTargetCenter.value(), scene, viewport),
                    VisibleTargetColour,
                    2.0F);
            }

            if (sensor.rememberedTargetFeet.has_value())
            {
                constexpr float MarkerRadius = 4.0F;
                const ImVec2 remembered =
                    screenPosition(sensor.rememberedTargetFeet.value(), scene, viewport);
                drawList.AddLine(observer, remembered, RememberedTargetColour, 1.5F);
                drawList.AddLine(
                    {remembered.x - MarkerRadius, remembered.y - MarkerRadius},
                    {remembered.x + MarkerRadius, remembered.y + MarkerRadius},
                    RememberedTargetColour,
                    2.0F);
                drawList.AddLine(
                    {remembered.x - MarkerRadius, remembered.y + MarkerRadius},
                    {remembered.x + MarkerRadius, remembered.y - MarkerRadius},
                    RememberedTargetColour,
                    2.0F);
                char memoryLabel[32]{};
                std::snprintf(memoryLabel, sizeof(memoryLabel), "%.2fs", sensor.memoryRemaining);
                drawList.AddText(
                    {remembered.x + MarkerRadius + 2.0F, remembered.y - MarkerRadius},
                    RememberedTargetColour,
                    memoryLabel);
            }
        }

        void drawActorPatrol(
            ImDrawList& drawList,
            const ActorDebugInfo& actor,
            const DebugOverlay& scene,
            const WindowViewport& viewport)
        {
            if (!actor.patrol.has_value())
            {
                return;
            }

            constexpr float PointRadius = 4.0F;
            constexpr float ActivePointRadius = 7.0F;
            const PatrolDebugInfo& patrol = actor.patrol.value();
            const ImVec2 first = screenPosition(patrol.firstFeet, scene, viewport);
            const ImVec2 second = screenPosition(patrol.secondFeet, scene, viewport);
            drawList.AddLine(first, second, PatrolRouteColour, 2.0F);
            drawList.AddCircleFilled(first, PointRadius, PatrolPointColour);
            drawList.AddCircleFilled(second, PointRadius, PatrolPointColour);
            drawList.AddCircle(
                patrol.headingToSecond ? second : first,
                ActivePointRadius,
                ActivePatrolPointColour,
                16,
                2.0F);

            const auto drawPatrolLabel = [&drawList, &actor](ImVec2 point, const char* pointName)
            {
                char actorLabel[32]{};
                std::snprintf(actorLabel, sizeof(actorLabel), "NPC %u", actor.id.value);
                const float lineHeight = ImGui::GetTextLineHeight();
                const float top = point.y - ActivePointRadius - 2.0F - lineHeight * 2.0F;
                const ImVec2 actorLabelSize = ImGui::CalcTextSize(actorLabel);
                const ImVec2 pointLabelSize = ImGui::CalcTextSize(pointName);
                drawList.AddText(
                    {point.x - actorLabelSize.x * 0.5F, top}, PatrolPointColour, actorLabel);
                drawList.AddText(
                    {point.x - pointLabelSize.x * 0.5F, top + lineHeight},
                    PatrolPointColour,
                    pointName);
            };
            drawPatrolLabel(first, "P1");
            drawPatrolLabel(second, "P2");
        }

        void drawActorWorldLabel(
            ImDrawList& drawList,
            const ActorDebugInfo& actor,
            const DebugOverlay& scene,
            const WindowViewport& viewport)
        {
            const glm::vec2 labelWorldPosition =
                actor.sprite.has_value() ? actor.sprite->bounds.position : actor.collider.position;
            ImVec2 labelPosition = screenPosition(labelWorldPosition, scene, viewport);
            const float lineHeight = ImGui::GetTextLineHeight();
            const std::string actorLabel = labelFor(actor);
            drawList.AddText(labelPosition, WorldLabelColour, actorLabel.c_str());

            if (actor.animation.has_value())
            {
                labelPosition.y += lineHeight;
                drawList.AddText(labelPosition, WorldLabelColour, nameOf(actor.animation.value()));
            }
            if (actor.npcState.has_value())
            {
                labelPosition.y += lineHeight;
                drawList.AddText(labelPosition, WorldLabelColour, nameOf(actor.npcState.value()));
            }
        }

        void drawProjectile(
            ImDrawList& drawList,
            const ProjectileDebugInfo& projectile,
            const DebugOverlay& scene,
            const WindowViewport& viewport)
        {
            drawWorldBounds(drawList, projectile.bounds, scene, viewport, ProjectileColour);

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
                    projectile.lifetimeRemaining);
            }
            else
            {
                std::snprintf(label, sizeof(label), "none\n%.2f", projectile.lifetimeRemaining);
            }
            drawList.AddText(labelPosition, ProjectileColour, label);
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

        void drawActorText(ImDrawList& drawList, const ActorDebugInfo& actor, ImVec2& position)
        {
            constexpr float Indentation = 12.0F;

            const std::string label = labelFor(actor);
            drawTextLine(drawList, position, label.c_str(), TextHeadingColour);

            char text[96]{};
            std::snprintf(
                text,
                sizeof(text),
                "pos:    %.1f, %.1f",
                actor.collider.position.x,
                actor.collider.position.y);
            drawTextLine(drawList, position, text, TextDetailColour, Indentation);

            if (actor.sprite.has_value())
            {
                std::snprintf(
                    text,
                    sizeof(text),
                    "frame:  %zu (%.0f, %.0f)",
                    actor.sprite->atlasFrame,
                    actor.sprite->atlasPosition.x,
                    actor.sprite->atlasPosition.y);
                drawTextLine(drawList, position, text, TextDetailColour, Indentation);
            }

            if (actor.pathFollower.has_value())
            {
                const PathFollowerDebugInfo& follower = actor.pathFollower.value();
                std::snprintf(text, sizeof(text), "repath: %.2f", follower.repathRemaining);
                drawTextLine(drawList, position, text, TextDetailColour, Indentation);
            }
            position.y += ActorTextGap;
        }

        float actorTextHeight(const ActorDebugInfo& actor)
        {
            int lineCount = 2;
            lineCount += actor.sprite.has_value() ? 1 : 0;
            lineCount += actor.pathFollower.has_value() ? 1 : 0;
            return static_cast<float>(lineCount) * ImGui::GetTextLineHeight() + ActorTextGap;
        }
    }

    void drawDebugOverlay(const DebugOverlay& scene, const std::optional<WindowViewport>& viewport)
    {
        ImDrawList* drawList = ImGui::GetBackgroundDrawList();

        if (viewport.has_value())
        {
            drawWorldBounds(*drawList, scene.cameraBounds, scene, *viewport, CameraBoundsColour);
            drawWorldBounds(
                *drawList, scene.cameraDeadZone, scene, *viewport, CameraDeadZoneColour);
            drawList->AddText(
                screenPosition(scene.cameraDeadZone.position, scene, *viewport),
                CameraDeadZoneColour,
                "camera dead zone");
        }

        constexpr float ActorTextWidth = 180.0F;
        constexpr float ActorTextMargin = 8.0F;
        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImVec2 actorTextPosition = {
            mainViewport->WorkPos.x + mainViewport->WorkSize.x - ActorTextWidth - ActorTextMargin,
            mainViewport->WorkPos.y + ActorTextMargin};
        const float actorTextBottom =
            mainViewport->WorkPos.y + mainViewport->WorkSize.y - ActorTextMargin;
        bool actorTextHasSpace = true;
        for (const ActorDebugInfo& actor : scene.actors)
        {
            if (actorTextHasSpace &&
                actorTextPosition.y + actorTextHeight(actor) <= actorTextBottom)
            {
                drawActorText(*drawList, actor, actorTextPosition);
            }
            else
            {
                actorTextHasSpace = false;
            }
            if (!viewport.has_value())
            {
                continue;
            }

            drawActorPatrol(*drawList, actor, scene, *viewport);
            drawActorSensor(*drawList, actor, scene, *viewport);
            drawActorPath(*drawList, actor, scene, *viewport);

            if (actor.biteHitbox.has_value())
            {
                drawWorldBounds(
                    *drawList, actor.biteHitbox.value(), scene, *viewport, BiteHitboxColour);
            }

            if (actor.sprite.has_value())
            {
                drawWorldBounds(
                    *drawList, actor.sprite->bounds, scene, *viewport, SpriteBoundsColour);
            }
            drawActorWorldLabel(*drawList, actor, scene, *viewport);
            drawWorldBounds(*drawList, actor.collider, scene, *viewport, ColliderBoundsColour);
        }

        if (viewport.has_value())
        {
            for (const ProjectileDebugInfo& projectile : scene.projectiles)
            {
                drawProjectile(*drawList, projectile, scene, *viewport);
            }
            if (scene.navigationCache.has_value())
            {
                for (const NavigationCellDebugInfo& cell : scene.navigationCache->cells)
                {
                    drawNavigationCell(*drawList, cell, scene, *viewport);
                }
            }
            for (const PickupDebugInfo& pickup : scene.pickups)
            {
                drawWorldBounds(*drawList, pickup.bounds, scene, *viewport, PickupColour);
                drawList->AddText(
                    screenPosition(pickup.bounds.position, scene, *viewport),
                    PickupColour,
                    pickup.itemName.c_str());
            }
        }
    }
}
