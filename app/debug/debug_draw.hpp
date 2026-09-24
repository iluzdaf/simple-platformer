#pragma once

#include <imgui.h>

#include <glm/vec2.hpp>

#include "simple_platformer/math/aabb.hpp"

namespace simple_platformer
{
    struct WindowViewport;

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

    // Where a world position lands on screen, given the camera's view of the world.
    ImVec2 screenPosition(
        glm::vec2 worldPosition,
        const Aabb& cameraBounds,
        const WindowViewport& viewport);

    void drawWorldBounds(
        ImDrawList& drawList,
        const Aabb& bounds,
        const Aabb& cameraBounds,
        const WindowViewport& viewport,
        ImU32 colour);

    // One line of the text column, moving the position down a line.
    void drawTextLine(
        ImDrawList& drawList,
        ImVec2& position,
        const char* text,
        ImU32 colour,
        float indentation = 0.0F);
}
