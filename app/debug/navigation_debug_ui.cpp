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
#include "ui/hud_draw.hpp"

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

        // A cell of the connection cache: filled while its connections are kept, and
        // outlined while they are missing.
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
        }

        void drawConnectionCount(
            ImDrawList& drawList,
            const NavigationCellDebugInfo& cell,
            const Aabb& cameraBounds,
            const WindowViewport& viewport)
        {
            if (!cell.connections.has_value())
            {
                return;
            }
            char count[8];
            std::snprintf(count, sizeof(count), "%zu", *cell.connections);
            const ImVec2 minimum = screenPosition(cell.bounds.position, cameraBounds, viewport);
            drawShadowedText(drawList, {minimum.x + 1.0F, minimum.y}, WorldLabelColour, count);
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
        const CursorCellDebugInfo cursor = cache.cursorCell.value_or(CursorCellDebugInfo{});
        const NavigationCellDebugInfo* cellUnderCursor = nullptr;
        for (const NavigationCellDebugInfo& cell : cache.cells)
        {
            drawNavigationCell(drawList, cell, cameraBounds, viewport);
            if (cache.cursorCell.has_value() && cell.bounds.position == cursor.bounds.position)
            {
                cellUnderCursor = &cell;
            }
        }
        if (cache.cursorCell.has_value())
        {
            drawCursorCell(drawList, cursor, cameraBounds, viewport);
        }
        if (cellUnderCursor != nullptr)
        {
            drawConnectionCount(drawList, *cellUnderCursor, cameraBounds, viewport);
        }
    }

    void drawNavigationTotals(
        ImDrawList& drawList,
        const NavigationCacheDebugInfo& cache,
        ImVec2& position)
    {
        // One value a line, keyed like the actor text, so every line fits the column.
        constexpr float Indentation = 12.0F;
        drawTextLine(drawList, position, "navigation cache", TextHeadingColour);
        char text[48];
        const auto line = [&](const char* key, std::size_t value)
        {
            std::snprintf(text, sizeof(text), "%-8s%zu", key, value);
            drawTextLine(drawList, position, text, TextDetailColour, Indentation);
        };
        if (cache.bodyName.empty())
        {
            std::snprintf(
                text,
                sizeof(text),
                "body:   %gx%g",
                static_cast<double>(cache.bodySize.x),
                static_cast<double>(cache.bodySize.y));
        }
        else
        {
            std::snprintf(text, sizeof(text), "body:   %s", cache.bodyName.c_str());
        }
        drawTextLine(drawList, position, text, TextDetailColour, Indentation);
        // The index and the key on a line of their own, since a name can fill the last.
        std::snprintf(
            text, sizeof(text), "shown:  %zu/%zu (N)", cache.bodyIndex + 1, cache.bodyCount);
        drawTextLine(drawList, position, text, TextDetailColour, Indentation);
        // Cells with connections, over every cell kept.
        std::snprintf(text, sizeof(text), "cells:  %zu/%zu", cache.cellsConnected, cache.cellsKept);
        drawTextLine(drawList, position, text, TextDetailColour, Indentation);
        line("pending:", cache.cellsPending);
        line("walks:", cache.walksKept);
        line("sets:", cache.reachableSetsKept);
        line("paths:", cache.pathsKept);
        line("breaks:", cache.breaksApplied);
        line("dropped:", cache.cellsDropped);
        line("kept:", cache.cellsKeptSoFar);
    }

    float navigationTotalsHeight()
    {
        return 11.0F * ImGui::GetTextLineHeight();
    }
}
