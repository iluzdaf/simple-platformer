#include "frame_profile_ui.hpp"

#include "frame_selection.hpp"

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <numeric>
#include <optional>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <implot.h>

#include "simple_platformer/timing/fixed_step.hpp"
#include "simple_platformer/timing/frame_profile.hpp"

namespace simple_platformer
{
    namespace
    {
        // Frame panel layout, in window pixels. Adjust these to resize the panel.
        constexpr float PanelWidth = 420.0F;
        // Tall enough for the legend beside it: the frame line, the budget, and one row per
        // simulation category.
        constexpr float PlotHeight = 160.0F;
        constexpr float LegendWidth = 130.0F;
        // A legend swatch sits inside its text row by this fraction of the row height.
        constexpr float SwatchInsetFraction = 0.2F;
        // A hidden series keeps its swatch at this fraction of its colour's opacity.
        constexpr float HiddenSwatchOpacity = 0.35F;
        // The plot column under the cursor, and the one picked.
        constexpr ImU32 HoveredFrameColour = IM_COL32(255, 255, 255, 90);
        constexpr ImU32 PickedFrameColour = IM_COL32(255, 255, 255, 230);

        // Frame plot scales. The frame axis keeps the 60 Hz budget in view, showing at
        // least this many budgets, and stretches to this much headroom over the worst frame.
        constexpr float TargetFrameMilliseconds = static_cast<float>(FixedDeltaSeconds) * 1000.0F;
        constexpr double FrameAxisBudgets = 2.0;
        constexpr double FrameAxisHeadroom = 1.1;
        // The stack's scale is a display choice, not a budget: an ordinary frame of this
        // project simulates in a fraction of it, and a slow frame goes off the top rather
        // than rescaling the axis under the reader. Raise it if the simulation grows.
        constexpr float SimulationAxisMilliseconds = 0.06F;

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
            bool hidden;
        };

        // The frame plot's legend, in a window of its own. It is the one part of the panel
        // the mouse can see, so a click on an entry toggles its series while a click
        // anywhere else over the panel reaches the game. Which series are hidden is kept
        // in the window's ImGui storage and filled into the series on the way out.
        void drawFramePlotLegend(ImVec2 topLeft, ImVec2 size, std::vector<FramePlotSeries>& series)
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
                ImGuiStorage* hidden = ImGui::GetStateStorage();
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const float rowHeight = ImGui::GetTextLineHeight();
                const float swatchInset = rowHeight * SwatchInsetFraction;
                const float swatchSize = rowHeight - 2.0F * swatchInset;
                for (FramePlotSeries& entry : series)
                {
                    const ImGuiID id = ImGui::GetID(entry.label);
                    entry.hidden = hidden->GetBool(id);
                    const ImVec2 rowTopLeft = ImGui::GetCursorScreenPos();
                    ImGui::PushID(entry.label);
                    if (ImGui::Selectable("##toggle", false, 0, {0.0F, rowHeight}))
                    {
                        entry.hidden = !entry.hidden;
                        hidden->SetBool(id, entry.hidden);
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
                    drawList->AddText(
                        {rowTopLeft.x + rowHeight + swatchInset, rowTopLeft.y},
                        ImGui::GetColorU32(entry.hidden ? ImGuiCol_TextDisabled : ImGuiCol_Text),
                        entry.label);
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
            if (!ImGui::BeginTable("phases", 2, ImGuiTableFlags_SizingFixedFit))
            {
                return;
            }
            // Both columns fit their content, so the numbers sit beside the longest name
            // instead of at the panel's far edge.
            ImGui::TableSetupColumn("phase", ImGuiTableColumnFlags_WidthFixed);
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
            ImGui::Separator();
            ImGui::Text(
                "simulation %6.3f ms per tick over %d ticks",
                std::accumulate(simulationSeconds.begin(), simulationSeconds.end(), 0.0F) * scale,
                ticks);
            ImGui::Text(
                "searches %d   remembered %d",
                history.totalPathSearches(),
                history.totalPathSearchesRemembered());
            ImGui::Text(
                "cells %d   reused %d   sim ticks %d",
                history.totalPathSearchNodes(),
                history.totalPathSearchCellsReused(),
                history.totalPathSearchSimulatedTicks());
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
                "searches %d   remembered %d", frame.pathSearches, frame.pathSearchesRemembered);
            ImGui::Text(
                "cells %d   reused %d   sim ticks %d",
                frame.pathSearchNodes,
                frame.pathSearchCellsReused,
                frame.pathSearchSimulatedTicks);
            drawPhaseTable(phasesByCost(frame.phases), "ms", 1000.0F);
        }
    }

    void drawFrameProfile(const FrameHistory& live, FrameSelection& selection)
    {
        const FrameHistory& history = selection.kept() != nullptr ? *selection.kept() : live;
        if (history.size() == 0)
        {
            return;
        }

        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(mainViewport->WorkPos, ImGuiCond_Always);
        // A fixed width keeps the window still while the numbers in it change; each line
        // below is written to fit it, and no scrollbar appears if one does not. The mouse
        // cannot see this window, so clicks over it reach the game like clicks over the
        // rest of the overlay; only the legend, a window of its own, takes them.
        ImGui::SetNextWindowSizeConstraints({PanelWidth, 0.0F}, {PanelWidth, FLT_MAX});
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

        const FrameProfile& worst = history.worst();
        // The phases come from every frame in the history, so one that runs only now and
        // then, such as a path search, keeps its row and band instead of coming and going.
        const std::vector<PhaseTiming> phases = history.phasesSummed();
        const int ticks = history.totalSimulationTicks();
        const std::vector<float> frameMilliseconds =
            toMilliseconds(history.frameSecondsOldestFirst());
        const int frameCount = static_cast<int>(frameMilliseconds.size());
        const auto frameAxis = static_cast<double>(history.capacity());
        const double frameTop = std::max(
            static_cast<double>(TargetFrameMilliseconds) * FrameAxisBudgets,
            static_cast<double>(worst.frameSeconds) * 1000.0 * FrameAxisHeadroom);
        constexpr ImPlotFlags PlotFlags = ImPlotFlags_NoInputs | ImPlotFlags_NoMenus |
                                          ImPlotFlags_NoTitle | ImPlotFlags_NoBoxSelect |
                                          ImPlotFlags_NoLegend;

        // The legend is drawn first so what it hides shapes this frame's plot.
        std::vector<FramePlotSeries> series = {
            {"60 Hz budget", ImPlot::GetColormapColor(0), false},
            {"frame", ImPlot::GetColormapColor(1), false}};
        if (ticks > 0)
        {
            for (const char* category : categoriesOf(phases))
            {
                series.push_back(
                    {category, ImPlot::GetColormapColor(static_cast<int>(series.size())), false});
            }
        }
        const ImVec2 panelTopLeft = ImGui::GetWindowPos();
        const ImVec2 padding = ImGui::GetStyle().WindowPadding;
        drawFramePlotLegend(
            {panelTopLeft.x + PanelWidth - padding.x - LegendWidth, panelTopLeft.y + padding.y},
            {LegendWidth, PlotHeight},
            series);
        const FramePlotSeries& budget = series[0];
        const FramePlotSeries& frame = series[1];

        ImVec2 plotTopLeft;
        ImVec2 plotSize;
        bool plotted = false;
        if (ImPlot::BeginPlot("##frame", {-LegendWidth, PlotHeight}, PlotFlags))
        {
            ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels);
            ImPlot::SetupAxis(ImAxis_Y1, "frame ms");
            ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, frameAxis, ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, frameTop, ImPlotCond_Always);
            if (ticks > 0)
            {
                // The simulation is a small fraction of a frame; on the frame axis its
                // stack would be a hairline, so it has an axis of its own on the right.
                ImPlot::SetupAxis(ImAxis_Y2, "simulation ms", ImPlotAxisFlags_Opposite);
                ImPlot::SetupAxisLimits(
                    ImAxis_Y2, 0.0, SimulationAxisMilliseconds, ImPlotCond_Always);
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
        if (plotted)
        {
            drawFramePicker(plotTopLeft, plotSize, history, live, selection);
        }

        const std::optional<std::size_t> picked = selection.selectedIndex();
        if (picked.has_value())
        {
            drawPickedFrame(selection.selectedFrame(), *picked, history.size());
        }
        else
        {
            drawHistorySummary(history, phases);
        }
        ImGui::End();
    }
}
