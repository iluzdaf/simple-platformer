#include "debug_overlay_ui.hpp"

#include "debug_draw.hpp"
#include "debug_overlay.hpp"
#include "navigation_debug.hpp"
#include "navigation_debug_ui.hpp"
#include "npc_names.hpp"
#include "graphics/display_viewport.hpp"

#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>

#include <imgui.h>

#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"
#include "simple_platformer/render/animation.hpp"
#include "ui/hud_draw.hpp"

namespace simple_platformer
{
    namespace
    {
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
                const ImVec2 from =
                    screenPosition(connection.fromFeet, scene.cameraBounds, viewport);
                const ImVec2 to = screenPosition(connection.toFeet, scene.cameraBounds, viewport);
                const ImU32 colour = pathColour(connection);
                const float thickness = connection.next ? 3.0F : 2.0F;
                if (connection.sampledFeet.size() >= 2)
                {
                    for (std::size_t sampleIndex = 1; sampleIndex < connection.sampledFeet.size();
                         ++sampleIndex)
                    {
                        drawList.AddLine(
                            screenPosition(
                                connection.sampledFeet[sampleIndex - 1],
                                scene.cameraBounds,
                                viewport),
                            screenPosition(
                                connection.sampledFeet[sampleIndex], scene.cameraBounds, viewport),
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
                drawShadowedText(
                    drawList,
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
                        screenPosition(labelWorldPosition, scene.cameraBounds, viewport);
                    const char* traversalName = nameOf(connection.traversal);
                    drawShadowedText(drawList, labelPosition, colour, traversalName);
                    drawList.AddLine(
                        screenPosition(feetOf(actor.collider), scene.cameraBounds, viewport),
                        to,
                        NextPathGuideColour);
                }
            }

            if (follower.destinationFeet.has_value())
            {
                constexpr float DestinationRadius = 6.0F;
                const ImVec2 destination =
                    screenPosition(follower.destinationFeet.value(), scene.cameraBounds, viewport);
                drawList.AddCircle(destination, DestinationRadius, PathDestinationColour, 16, 2.0F);
                drawShadowedText(
                    drawList,
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
            const ImVec2 observer =
                screenPosition(sensor.observerCenter, scene.cameraBounds, viewport);
            drawList.AddCircle(
                observer, sensor.noticeDistance * viewport.scale.x, SensorRangeColour, 48, 1.0F);

            if (sensor.visibleTargetCenter.has_value())
            {
                drawList.AddLine(
                    observer,
                    screenPosition(
                        sensor.visibleTargetCenter.value(), scene.cameraBounds, viewport),
                    VisibleTargetColour,
                    2.0F);
            }

            if (sensor.rememberedTargetFeet.has_value())
            {
                constexpr float MarkerRadius = 4.0F;
                const ImVec2 remembered = screenPosition(
                    sensor.rememberedTargetFeet.value(), scene.cameraBounds, viewport);
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
                drawShadowedText(
                    drawList,
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
            const ImVec2 first = screenPosition(patrol.firstFeet, scene.cameraBounds, viewport);
            const ImVec2 second = screenPosition(patrol.secondFeet, scene.cameraBounds, viewport);
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
                drawShadowedText(
                    drawList,
                    {point.x - actorLabelSize.x * 0.5F, top},
                    PatrolPointColour,
                    actorLabel);
                drawShadowedText(
                    drawList,
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
            ImVec2 labelPosition = screenPosition(labelWorldPosition, scene.cameraBounds, viewport);
            const float lineHeight = ImGui::GetTextLineHeight();
            const std::string actorLabel = labelFor(actor);
            drawShadowedText(drawList, labelPosition, WorldLabelColour, actorLabel.c_str());

            if (actor.animation.has_value())
            {
                labelPosition.y += lineHeight;
                drawShadowedText(
                    drawList, labelPosition, WorldLabelColour, nameOf(actor.animation.value()));
            }
            if (actor.npcState.has_value())
            {
                labelPosition.y += lineHeight;
                drawShadowedText(
                    drawList, labelPosition, WorldLabelColour, nameOf(actor.npcState.value()));
            }
        }

        void drawProjectile(
            ImDrawList& drawList,
            const ProjectileDebugInfo& projectile,
            const DebugOverlay& scene,
            const WindowViewport& viewport)
        {
            drawWorldBounds(
                drawList, projectile.bounds, scene.cameraBounds, viewport, ProjectileColour);

            ImVec2 labelPosition =
                screenPosition(projectile.bounds.position, scene.cameraBounds, viewport);
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
            drawShadowedText(drawList, labelPosition, ProjectileColour, label);
        }

        // Outlines the breakable cell under the cursor and names the key that breaks it.
        void drawBreakableCellHint(
            ImDrawList& drawList,
            const Aabb& cell,
            const DebugOverlay& scene,
            const WindowViewport& viewport)
        {
            drawWorldBounds(drawList, cell, scene.cameraBounds, viewport, WorldLabelColour);
            const ImVec2 above = screenPosition(cell.position, scene.cameraBounds, viewport);
            drawShadowedText(
                drawList,
                {above.x, above.y - ImGui::GetTextLineHeight()},
                WorldLabelColour,
                "B to break");
        }

        // A second box round the NPC whose machine the machine window shows.
        void drawFollowedOutline(
            ImDrawList& drawList,
            const ActorDebugInfo& actor,
            const DebugOverlay& scene,
            const WindowViewport& viewport)
        {
            constexpr float Gap = 2.0F;
            const Aabb outline{
                actor.collider.position - glm::vec2{Gap, Gap},
                actor.collider.size + glm::vec2{Gap, Gap} * 2.0F};
            drawWorldBounds(drawList, outline, scene.cameraBounds, viewport, FollowedActorColour);
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

            if (actor.npcTactic.has_value())
            {
                std::snprintf(text, sizeof(text), "tactic: %s", nameOf(actor.npcTactic.value()));
                drawTextLine(drawList, position, text, TextDetailColour, Indentation);
            }
            if (actor.machine.has_value() && actor.machineState.has_value())
            {
                std::snprintf(
                    text,
                    sizeof(text),
                    "machine: %s, %s",
                    actor.machine->c_str(),
                    actor.machineState->c_str());
                drawTextLine(drawList, position, text, TextDetailColour, Indentation);
            }
            position.y += ActorTextGap;
        }

        float actorTextHeight(const ActorDebugInfo& actor)
        {
            int lineCount = 2;
            lineCount += actor.sprite.has_value() ? 1 : 0;
            lineCount += actor.npcTactic.has_value() ? 1 : 0;
            lineCount += actor.machine.has_value() ? 1 : 0;
            return static_cast<float>(lineCount) * ImGui::GetTextLineHeight() + ActorTextGap;
        }
    }

    void drawDebugOverlay(const DebugOverlay& scene, const std::optional<WindowViewport>& viewport)
    {
        ImDrawList* drawList = ImGui::GetBackgroundDrawList();

        if (viewport.has_value())
        {
            drawWorldBounds(
                *drawList, scene.cameraBounds, scene.cameraBounds, *viewport, CameraBoundsColour);
            drawWorldBounds(
                *drawList,
                scene.cameraDeadZone,
                scene.cameraBounds,
                *viewport,
                CameraDeadZoneColour);
            drawShadowedText(
                *drawList,
                screenPosition(scene.cameraDeadZone.position, scene.cameraBounds, *viewport),
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
                    *drawList,
                    actor.biteHitbox.value(),
                    scene.cameraBounds,
                    *viewport,
                    BiteHitboxColour);
            }

            if (actor.sprite.has_value())
            {
                drawWorldBounds(
                    *drawList,
                    actor.sprite->bounds,
                    scene.cameraBounds,
                    *viewport,
                    SpriteBoundsColour);
            }
            drawActorWorldLabel(*drawList, actor, scene, *viewport);
            drawWorldBounds(
                *drawList, actor.collider, scene.cameraBounds, *viewport, ColliderBoundsColour);
            if (scene.machine.has_value() && scene.machine->actor == actor.id)
            {
                drawFollowedOutline(*drawList, actor, scene, *viewport);
            }
        }

        if (scene.navigationCache.has_value() && actorTextHasSpace &&
            actorTextPosition.y + navigationTotalsHeight() <= actorTextBottom)
        {
            drawNavigationTotals(
                *drawList,
                scene.navigationCache.value_or(NavigationCacheDebugInfo{}),
                actorTextPosition);
        }

        if (viewport.has_value())
        {
            for (const ProjectileDebugInfo& projectile : scene.projectiles)
            {
                drawProjectile(*drawList, projectile, scene, *viewport);
            }
            if (scene.navigationCache.has_value())
            {
                drawNavigationCache(
                    *drawList,
                    scene.navigationCache.value_or(NavigationCacheDebugInfo{}),
                    scene.cameraBounds,
                    *viewport);
            }
            if (scene.breakableCellUnderCursor.has_value())
            {
                drawBreakableCellHint(
                    *drawList, scene.breakableCellUnderCursor.value_or(Aabb{}), scene, *viewport);
            }
            for (const PickupDebugInfo& pickup : scene.pickups)
            {
                drawWorldBounds(
                    *drawList, pickup.bounds, scene.cameraBounds, *viewport, PickupColour);
                drawShadowedText(
                    *drawList,
                    screenPosition(pickup.bounds.position, scene.cameraBounds, *viewport),
                    PickupColour,
                    pickup.itemName.c_str());
            }
        }
    }
}
