#include "navigation_debug_ui.hpp"

#include <cstddef>
#include <cstdio>
#include <optional>
#include <vector>

#include <imgui.h>

#include <glm/vec2.hpp>

#include "debug_draw.hpp"
#include "graphics/display_viewport.hpp"
#include "navigation_debug.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/navigation/navigation_path.hpp"

namespace simple_platformer
{
    namespace
    {
        // The connection cache's cells: kept with connections, kept with none, and missing,
        // which after a break means dropped and not yet simulated again.
        constexpr ImU32 NavigationKeptColour = IM_COL32(64, 160, 255, 90);
        constexpr ImU32 NavigationEmptyColour = IM_COL32(128, 128, 128, 70);
        constexpr ImU32 NavigationMissingColour = IM_COL32(255, 96, 32, 220);
        // The cursor cell's footprint, and the cells reachable from it.
        constexpr ImU32 NavigationFootprintColour = IM_COL32(255, 224, 64, 200);
        constexpr ImU32 NavigationReachableColour = IM_COL32(64, 224, 255, 50);

        ImU32 traversalColour(Traversal traversal)
        {
            switch (traversal)
            {
            case Traversal::Walk:
                return WalkingPathColour;
            case Traversal::Fall:
                return FallingPathColour;
            case Traversal::Jump:
                return JumpingPathColour;
            case Traversal::Fly:
                return FlyingPathColour;
            }
            return UnknownPathColour;
        }

        // A cell of the connection cache: a filled cell while its connections are kept,
        // with their count, and an outlined one while they are missing.
        void drawNavigationCell(
            ImDrawList& drawList,
            const NavigationCellDebugInfo& cell,
            const Aabb& cameraBounds,
            const WindowViewport& viewport)
        {
            const ImVec2 minimum = screenPosition(cell.bounds.position, cameraBounds, viewport);
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

        // What the cache holds and has done, in the bottom left corner, where neither
        // the frame panel nor the actor text sits.
        void drawNavigationTotals(ImDrawList& drawList, const NavigationCacheDebugInfo& cache)
        {
            constexpr float Margin = 8.0F;
            char lines[3][96];
            std::snprintf(
                lines[0],
                sizeof(lines[0]),
                "navigation cache  body %zu of %zu  %gx%g  (N for next)",
                cache.bodyIndex + 1,
                cache.bodyCount,
                static_cast<double>(cache.bodySize.x),
                static_cast<double>(cache.bodySize.y));
            std::snprintf(
                lines[1],
                sizeof(lines[1]),
                "cells %zu  reachable sets %zu  paths %zu",
                cache.cellsKept,
                cache.reachableSetsKept,
                cache.pathsKept);
            std::snprintf(
                lines[2],
                sizeof(lines[2]),
                "breaks %zu  cells dropped %zu  cells kept so far %zu",
                cache.breaksApplied,
                cache.cellsDropped,
                cache.cellsKeptSoFar);
            const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
            const float lineHeight = ImGui::GetTextLineHeight();
            ImVec2 position = {
                mainViewport->WorkPos.x + Margin,
                mainViewport->WorkPos.y + mainViewport->WorkSize.y - Margin - 3.0F * lineHeight};
            for (const char* line : lines)
            {
                drawList.AddText(position, TextDetailColour, line);
                position.y += lineHeight;
            }
        }

        // The cursor cell: its reachable cells shaded, its footprint outlined, and each
        // connection drawn to where it lands, jumps and falls along their arcs.
        void drawCursorCell(
            ImDrawList& drawList,
            const CursorCellDebugInfo& cell,
            const Aabb& cameraBounds,
            const WindowViewport& viewport)
        {
            for (const Aabb& reachable : cell.reachable)
            {
                const ImVec2 minimum = screenPosition(reachable.position, cameraBounds, viewport);
                drawList.AddRectFilled(
                    minimum,
                    {minimum.x + reachable.size.x * viewport.scale.x,
                     minimum.y + reachable.size.y * viewport.scale.y},
                    NavigationReachableColour);
            }
            if (cell.footprint.has_value())
            {
                drawWorldBounds(
                    drawList,
                    cell.footprint.value_or(Aabb{}),
                    cameraBounds,
                    viewport,
                    NavigationFootprintColour);
            }
            for (const CachedConnectionDebugInfo& connection : cell.connections)
            {
                const ImU32 colour = traversalColour(connection.traversal);
                if (connection.sampledFeet.size() < 2)
                {
                    drawList.AddLine(
                        screenPosition(connection.fromFeet, cameraBounds, viewport),
                        screenPosition(connection.toFeet, cameraBounds, viewport),
                        colour,
                        1.0F);
                    continue;
                }
                for (std::size_t index = 1; index < connection.sampledFeet.size(); ++index)
                {
                    drawList.AddLine(
                        screenPosition(connection.sampledFeet[index - 1], cameraBounds, viewport),
                        screenPosition(connection.sampledFeet[index], cameraBounds, viewport),
                        colour,
                        1.0F);
                }
            }
            drawWorldBounds(drawList, cell.bounds, cameraBounds, viewport, WorldLabelColour);
        }
    }

    void drawNavigationCache(
        ImDrawList& drawList,
        const NavigationCacheDebugInfo& cache,
        const Aabb& cameraBounds,
        const WindowViewport& viewport)
    {
        for (const NavigationCellDebugInfo& cell : cache.cells)
        {
            drawNavigationCell(drawList, cell, cameraBounds, viewport);
        }
        if (cache.cursorCell.has_value())
        {
            drawCursorCell(
                drawList, cache.cursorCell.value_or(CursorCellDebugInfo{}), cameraBounds, viewport);
        }
        drawNavigationTotals(drawList, cache);
    }
}
