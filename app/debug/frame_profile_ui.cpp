#include "frame_profile_ui.hpp"

#include "frame_axes.hpp"
#include "frame_selection.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <optional>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <implot.h>

#include "simple_platformer/timing/fixed_step.hpp"
#include "simple_platformer/timing/frame_profile.hpp"
#include "ui/hud_draw.hpp"

namespace simple_platformer
{
    namespace
    {
        // Frame panel layout, in window pixels. Adjust these to resize the panel.
        constexpr float PanelWidth = 420.0F;
        constexpr float PlotHeight = 160.0F;
        constexpr int LegendColumns = 3;
        // A legend swatch sits inside its text row by this fraction of the row height.
        constexpr float SwatchInsetFraction = 0.2F;
        // A hidden series keeps its swatch at this fraction of its colour's opacity.
        constexpr float HiddenSwatchOpacity = 0.35F;
        // The plot column under the cursor, and the one picked.
        constexpr ImU32 HoveredFrameColour = IM_COL32(255, 255, 255, 90);
        constexpr ImU32 PickedFrameColour = IM_COL32(255, 255, 255, 230);

        // The fixed-step budget shown on the frame-time axis.
        constexpr float TargetFrameMilliseconds = static_cast<float>(FixedDeltaSeconds) * 1000.0F;

        std::vector<float> toMilliseconds(std::vector<float> seconds)
        {
            for (float& value : seconds)
            {
                value *= 1000.0F;
            }
            return seconds;
        }

        // The categories of the phases, in the order the simulation first charged them.
        std::vector<const char*> categoriesOf(const std::vector<PhaseTiming>& phases)
        {
            std::vector<const char*> categories;
            for (const PhaseTiming& phase : phases)
            {
                const bool seen = std::any_of(
                    categories.begin(),
                    categories.end(),
                    [&](const char* category)
                    { return std::string_view(category) == phase.category; });
                if (!seen)
                {
                    categories.push_back(phase.category);
                }
            }
            return categories;
        }

        // One series of the frame plot: what the legend shows and how the plot draws it.
        struct FramePlotSeries
        {
            const char* label;
            ImVec4 colour;
            ImGuiID storageId;
            bool hidden;
        };

        // The frame plot's legend, in a short window below the plot. A click on an entry
        // toggles its series. Hidden state belongs to the plot window, which exists even
        // when the details are collapsed, so hiding the legend does not reset the plot.
        void drawFramePlotLegend(
            ImVec2 topLeft,
            ImVec2 size,
            ImGuiStorage& hidden,
            std::vector<FramePlotSeries>& series)
        {
            ImGui::SetNextWindowPos(topLeft, ImGuiCond_Always);
            ImGui::SetNextWindowSize(size, ImGuiCond_Always);
            if (ImGui::Begin(
                    "Frame legend##profile",
                    nullptr,
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse))
            {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const float rowHeight = ImGui::GetTextLineHeight();
                const float swatchInset = rowHeight * SwatchInsetFraction;
                const float swatchSize = rowHeight - 2.0F * swatchInset;
                if (ImGui::BeginTable(
                        "series",
                        LegendColumns,
                        ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX))
                {
                    for (std::size_t index = 0; index < series.size(); ++index)
                    {
                        FramePlotSeries& entry = series[index];
                        if (index % LegendColumns == 0)
                        {
                            ImGui::TableNextRow(0, rowHeight);
                        }
                        ImGui::TableSetColumnIndex(static_cast<int>(index % LegendColumns));
                        const ImVec2 rowTopLeft = ImGui::GetCursorScreenPos();
                        ImGui::PushID(entry.label);
                        if (ImGui::Selectable("##toggle", false, 0, {0.0F, rowHeight}))
                        {
                            entry.hidden = !entry.hidden;
                            hidden.SetBool(entry.storageId, entry.hidden);
                        }
                        ImGui::PopID();
                        ImVec4 swatch = entry.colour;
                        if (entry.hidden)
                        {
                            swatch.w *= HiddenSwatchOpacity;
                        }
                        drawList->AddRectFilled(
                            {rowTopLeft.x + swatchInset, rowTopLeft.y + swatchInset},
                            {rowTopLeft.x + swatchInset + swatchSize,
                             rowTopLeft.y + swatchInset + swatchSize},
                            ImGui::ColorConvertFloat4ToU32(swatch));
                        drawShadowedText(
                            *drawList,
                            {rowTopLeft.x + rowHeight + swatchInset, rowTopLeft.y},
                            ImGui::GetColorU32(
                                entry.hidden ? ImGuiCol_TextDisabled : ImGuiCol_Text),
                            entry.label);
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::End();
        }

        // The plot's picker, a window of its own over the plot area like the legend, so a
        // press there picks the frame under the cursor while clicks over the rest of the
        // panel reach the game. Holding the button scrubs: the pick follows the cursor
        // until it is released. A click on the picked frame that never moves off it lets
        // the live history show again; whether the press landed on it is kept in the
        // window's ImGui storage until the release, as the legend keeps what it hides.
        // The column under the cursor and the picked one are marked.
        void drawFramePicker(
            ImVec2 topLeft,
            ImVec2 size,
            const FrameHistory& history,
            const FrameHistory& live,
            FrameSelection& selection)
        {
            ImGui::SetNextWindowPos(topLeft, ImGuiCond_Always);
            ImGui::SetNextWindowSize(size, ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0F, 0.0F});
            ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, {0.0F, 0.0F});
            if (ImGui::Begin(
                    "Frame picker##profile",
                    nullptr,
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse))
            {
                ImGui::InvisibleButton("Pick frame", size);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const auto markFrame = [&](std::size_t index, ImU32 colour)
                {
                    const float x = topLeft.x + size.x * static_cast<float>(index) /
                                                    static_cast<float>(history.capacity());
                    drawList->AddLine({x, topLeft.y}, {x, topLeft.y + size.y}, colour);
                };
                const float fraction = (ImGui::GetMousePos().x - topLeft.x) / size.x;
                ImGuiStorage* storage = ImGui::GetStateStorage();
                const ImGuiID pressedOnPicked = ImGui::GetID("pressed on picked");
                if (ImGui::IsItemActive())
                {
                    const std::size_t under =
                        frameNearestPlotFraction(fraction, history.capacity(), history.size());
                    if (ImGui::IsItemActivated())
                    {
                        storage->SetBool(pressedOnPicked, selection.selectedIndex() == under);
                    }
                    if (selection.selectedIndex() != under)
                    {
                        selection.select(live, under);
                        storage->SetBool(pressedOnPicked, false);
                    }
                }
                else if (ImGui::IsItemDeactivated() && storage->GetBool(pressedOnPicked))
                {
                    selection.clear();
                }
                else if (ImGui::IsItemHovered())
                {
                    const std::optional<std::size_t> hovered =
                        frameAtPlotFraction(fraction, history.capacity(), history.size());
                    if (hovered.has_value())
                    {
                        markFrame(*hovered, HoveredFrameColour);
                    }
                }
                const std::optional<std::size_t> picked = selection.selectedIndex();
                if (picked.has_value())
                {
                    markFrame(*picked, PickedFrameColour);
                }
            }
            ImGui::End();
            ImGui::PopStyleVar(2);
        }

        // Each category with its total, then its phases, in the order given: simulation
        // order for the live history, so rows never move while the numbers change, and by
        // cost for a picked frame. Seconds are multiplied by the scale before they are
        // shown: a thousand for milliseconds, or a thousand over the tick count for
        // milliseconds per simulation step.
        void drawPhaseTable(
            const std::vector<PhaseTiming>& phases,
            const char* heading,
            float scale)
        {
            if (!ImGui::BeginTable("phases", 2, ImGuiTableFlags_SizingStretchProp))
            {
                return;
            }
            // Names fill the available width while the measurement stays compact at the
            // right edge, so the breakdown shares the full width of the plot above it.
            ImGui::TableSetupColumn("phase", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(heading, ImGuiTableColumnFlags_WidthFixed);
            for (const char* category : categoriesOf(phases))
            {
                float categorySeconds = 0.0F;
                for (const PhaseTiming& phase : phases)
                {
                    if (std::string_view(phase.category) == category)
                    {
                        categorySeconds += phase.seconds;
                    }
                }
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(category);
                ImGui::TableNextColumn();
                ImGui::Text("%6.3f", categorySeconds * scale);
                for (const PhaseTiming& phase : phases)
                {
                    if (std::string_view(phase.category) != category)
                    {
                        continue;
                    }
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("   %s", phase.name);
                    ImGui::TableNextColumn();
                    ImGui::Text("%6.3f", phase.seconds * scale);
                }
            }
            ImGui::EndTable();
        }

        // The history's summary under the plot: the latest frame, the average and the
        // worst, then the simulation's costs per step over every frame held.
        void drawHistorySummary(const FrameHistory& history, const std::vector<PhaseTiming>& phases)
        {
            const FrameProfile& latest = history.latest();
            ImGui::Text(
                "frame %6.2f ms   avg %6.2f   worst %6.2f",
                latest.frameSeconds * 1000.0F,
                history.averageFrameSeconds() * 1000.0F,
                history.worst().frameSeconds * 1000.0F);
            ImGui::Text(
                "scene %5.2f ms   render %5.2f   ui %5.2f",
                latest.sceneSeconds * 1000.0F,
                latest.renderSeconds * 1000.0F,
                latest.interfaceSeconds * 1000.0F);
            const int ticks = history.totalSimulationTicks();
            if (ticks == 0)
            {
                return;
            }

            // The list averages each cost per simulation step over the whole history. One
            // frame's numbers change sixty times a second and a phase of a few microseconds
            // would flicker between 0.00 and 0.01; the stack above already shows the spikes.
            const std::vector<float> simulationSeconds = history.simulationSecondsOldestFirst();
            const float scale = 1000.0F / static_cast<float>(ticks);
            ImGui::Text(
                "simulation %6.3f ms per tick over %d ticks",
                std::accumulate(simulationSeconds.begin(), simulationSeconds.end(), 0.0F) * scale,
                ticks);
            ImGui::Text(
                "searches %d   remembered %d   deferred %d",
                history.totalPathSearches(),
                history.totalPathSearchesRemembered(),
                history.totalPathSearchesDeferred());
            ImGui::Text(
                "cells %d   reused %d",
                history.totalPathSearchNodes(),
                history.totalPathSearchCellsReused());
            ImGui::Text(
                "sim ticks %d   fill ticks %d",
                history.totalPathSearchSimulatedTicks(),
                history.totalNavigationFillTicks());
            drawPhaseTable(phases, "ms per tick", scale);
        }

        // One picked frame's own costs, whole rather than per step and listed from the
        // dearest, since a still frame can be read at leisure.
        void drawPickedFrame(const FrameProfile& frame, std::size_t index, std::size_t count)
        {
            ImGui::Text(
                "frame %d of %d   %6.2f ms",
                static_cast<int>(index + 1),
                static_cast<int>(count),
                frame.frameSeconds * 1000.0F);
            ImGui::Text(
                "scene %5.2f ms   render %5.2f   ui %5.2f",
                frame.sceneSeconds * 1000.0F,
                frame.renderSeconds * 1000.0F,
                frame.interfaceSeconds * 1000.0F);
            if (frame.simulationTicks == 0)
            {
                return;
            }
            ImGui::Separator();
            ImGui::Text(
                "simulation %6.3f ms over %d ticks",
                frame.simulationSeconds * 1000.0F,
                frame.simulationTicks);
            ImGui::Text(
                "searches %d   remembered %d   deferred %d",
                frame.pathSearches,
                frame.pathSearchesRemembered,
                frame.pathSearchesDeferred);
            ImGui::Text("cells %d   reused %d", frame.pathSearchNodes, frame.pathSearchCellsReused);
            ImGui::Text(
                "sim ticks %d   fill ticks %d",
                frame.pathSearchSimulatedTicks,
                frame.navigationFillTicks);
            drawPhaseTable(phasesByCost(frame.phases), "ms", 1000.0F);
        }

        // The breakdown fills the window below the plot and legend. It still scrolls
        // with the wheel when its phase list is longer, but does not draw a scrollbar.
        void drawFrameDetails(
            ImVec2 topLeft,
            float width,
            float height,
            const FrameHistory& history,
            const std::vector<PhaseTiming>& phases,
            const FrameSelection& selection)
        {
            ImGui::SetNextWindowPos(topLeft, ImGuiCond_Always);
            ImGui::SetNextWindowSize({width, height}, ImGuiCond_Always);
            if (ImGui::Begin(
                    "Frame details##profile",
                    nullptr,
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar))
            {
                const std::optional<std::size_t> picked = selection.selectedIndex();
                if (picked.has_value())
                {
                    drawPickedFrame(selection.selectedFrame(), *picked, history.size());
                }
                else
                {
                    drawHistorySummary(history, phases);
                }
            }
            ImGui::End();
        }
    }

    void drawFrameProfile(
        const FrameHistory& live,
        FrameSelection& selection,
        FrameAxes& axes,
        bool showDetails)
    {
        const FrameHistory& history = selection.kept() != nullptr ? *selection.kept() : live;
        if (history.size() == 0)
        {
            return;
        }

        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(mainViewport->WorkPos, ImGuiCond_Always);
        // A fixed width keeps the plot still while the numbers change. The mouse cannot
        // see this window, so clicks over it reach the game like clicks over the rest of
        // the overlay; the picker and optional lower panel are windows of their own.
        ImGui::SetNextWindowSizeConstraints(
            {PanelWidth, 0.0F}, {PanelWidth, mainViewport->WorkSize.y});
        if (!ImGui::Begin(
                "Frame##profile",
                nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoMouseInputs))
        {
            ImGui::End();
            return;
        }

        // The phases come from every frame in the history, so one that runs only now and
        // then, such as a path search, keeps its row and band instead of coming and going.
        const std::vector<PhaseTiming> phases = history.phasesSummed();
        const int ticks = history.totalSimulationTicks();
        const std::vector<float> frameMilliseconds =
            toMilliseconds(history.frameSecondsOldestFirst());
        const int frameCount = static_cast<int>(frameMilliseconds.size());
        const auto frameAxis = static_cast<double>(history.capacity());
        // A picked frame keeps the plotted history still; keep its scale still as well so
        // live spikes cannot move the data under inspection. Fitting resumes when the
        // selection is cleared.
        if (selection.kept() == nullptr)
        {
            axes.fitTo(live);
        }
        constexpr ImPlotFlags PlotFlags = ImPlotFlags_NoInputs | ImPlotFlags_NoMenus |
                                          ImPlotFlags_NoTitle | ImPlotFlags_NoBoxSelect |
                                          ImPlotFlags_NoLegend | ImPlotFlags_NoFrame;

        ImGuiStorage& hidden = *ImGui::GetStateStorage();
        std::vector<FramePlotSeries> series;
        const auto addSeries = [&](const char* label, ImVec4 colour)
        {
            ImGui::PushID("Frame plot series");
            const ImGuiID storageId = ImGui::GetID(label);
            ImGui::PopID();
            series.push_back({label, colour, storageId, hidden.GetBool(storageId)});
        };
        addSeries("60 Hz budget", ImPlot::GetColormapColor(0));
        addSeries("frame", ImPlot::GetColormapColor(1));
        if (ticks > 0)
        {
            for (const char* category : categoriesOf(phases))
            {
                addSeries(category, ImPlot::GetColormapColor(static_cast<int>(series.size())));
            }
        }
        const ImVec2 padding = ImGui::GetStyle().WindowPadding;
        const ImVec2 plotWidgetTopLeft = ImGui::GetCursorScreenPos();
        const float plotWidgetWidth = ImGui::GetContentRegionAvail().x;
        const auto legendRows = (series.size() + static_cast<std::size_t>(LegendColumns) - 1) /
                                static_cast<std::size_t>(LegendColumns);
        const float legendHeight =
            2.0F * padding.y + ImGui::GetTextLineHeight() * static_cast<float>(legendRows);
        const FramePlotSeries& budget = series[0];
        const FramePlotSeries& frame = series[1];

        ImVec2 plotTopLeft;
        ImVec2 plotSize;
        bool plotted = false;
        ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImVec4{0.0F, 0.0F, 0.0F, 0.0F});
        ImPlot::PushStyleVar(ImPlotStyleVar_PlotBorderSize, 0.0F);
        if (ImPlot::BeginPlot("##frame", {plotWidgetWidth, PlotHeight}, PlotFlags))
        {
            ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels);
            ImPlot::SetupAxis(ImAxis_Y1, "frame ms");
            ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, frameAxis, ImPlotCond_Always);
            ImPlot::SetupAxisLimits(
                ImAxis_Y1,
                0.0,
                static_cast<double>(axes.frameTopMilliseconds()),
                ImPlotCond_Always);
            if (ticks > 0)
            {
                // The simulation is a small fraction of a frame; on the frame axis its
                // stack would be a hairline, so it has an axis of its own on the right.
                ImPlot::SetupAxis(ImAxis_Y2, "simulation ms", ImPlotAxisFlags_Opposite);
                ImPlot::SetupAxisLimits(
                    ImAxis_Y2,
                    0.0,
                    static_cast<double>(axes.simulationTopMilliseconds()),
                    ImPlotCond_Always);
            }

            if (!budget.hidden)
            {
                ImPlot::PlotInfLines(
                    budget.label,
                    &TargetFrameMilliseconds,
                    1,
                    ImPlotSpec(
                        ImPlotProp_Flags,
                        ImPlotInfLinesFlags_Horizontal,
                        ImPlotProp_LineColor,
                        budget.colour));
            }
            if (!frame.hidden)
            {
                ImPlot::PlotLine(
                    frame.label,
                    frameMilliseconds.data(),
                    frameCount,
                    1.0,
                    0.0,
                    ImPlotSpec(ImPlotProp_LineColor, frame.colour));
            }

            if (ticks > 0)
            {
                ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
                std::vector<float> frames(frameMilliseconds.size());
                for (std::size_t index = 0; index < frames.size(); ++index)
                {
                    frames[index] = static_cast<float>(index);
                }
                std::vector<float> lower(frames.size(), 0.0F);
                for (std::size_t index = 2; index < series.size(); ++index)
                {
                    const FramePlotSeries& category = series[index];
                    // A hidden category leaves the stack, so the bands above it drop to
                    // the cost that remains instead of floating over a gap.
                    if (category.hidden)
                    {
                        continue;
                    }
                    std::vector<float> upper =
                        toMilliseconds(history.categorySecondsOldestFirst(category.label));
                    for (std::size_t frameIndex = 0; frameIndex < upper.size(); ++frameIndex)
                    {
                        upper[frameIndex] += lower[frameIndex];
                    }
                    ImPlot::PlotShaded(
                        category.label,
                        frames.data(),
                        lower.data(),
                        upper.data(),
                        frameCount,
                        ImPlotSpec(ImPlotProp_FillColor, category.colour));
                    lower = upper;
                }
            }
            // Asking for the plot area locks its setup, so it comes after the axes.
            plotTopLeft = ImPlot::GetPlotPos();
            plotSize = ImPlot::GetPlotSize();
            plotted = true;
            ImPlot::EndPlot();
        }
        ImPlot::PopStyleVar();
        ImPlot::PopStyleColor();
        if (plotted)
        {
            drawFramePicker(plotTopLeft, plotSize, history, live, selection);
        }

        if (plotted && showDetails)
        {
            // ImPlot's visible data rectangle is narrower than its widget because the
            // axes live inside the widget. Expand the lower windows by their padding so
            // their content edges, rather than their invisible window edges, line up
            // with that rectangle.
            const ImVec2 legendTopLeft = {
                plotTopLeft.x - padding.x, plotWidgetTopLeft.y + PlotHeight + padding.y};
            const float lowerWindowWidth = plotSize.x + 2.0F * padding.x;
            drawFramePlotLegend(legendTopLeft, {lowerWindowWidth, legendHeight}, hidden, series);
            const ImVec2 detailsTopLeft = {legendTopLeft.x, legendTopLeft.y + legendHeight};
            const float availableHeight =
                mainViewport->WorkPos.y + mainViewport->WorkSize.y - detailsTopLeft.y;
            if (availableHeight > 0.0F)
            {
                drawFrameDetails(
                    detailsTopLeft, lowerWindowWidth, availableHeight, history, phases, selection);
            }
        }
        ImGui::End();
    }
}
