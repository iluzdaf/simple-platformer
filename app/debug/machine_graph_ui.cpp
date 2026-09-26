#include "machine_graph_ui.hpp"

#include "debug_overlay.hpp"
#include "debug_ui_layout.hpp"
#include "npc_names.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include <imgui.h>
#include <imgui_node_editor.h>

#include "simple_platformer/npc/npc_state_machine.hpp"

namespace ed = ax::NodeEditor;

namespace simple_platformer
{
    namespace
    {
        constexpr float RingRadiusPerState = 20.0F;
        constexpr float RingRadiusLeast = 120.0F;
        constexpr float WindowHeightFraction = 2.0F / 3.0F;
        constexpr float LinkThickness = 1.5F;
        constexpr float FiredLinkThickness = 3.0F;
        constexpr float ActiveBorderWidth = 3.0F;
        constexpr float PinHitSize = 12.0F;
        constexpr float PinRadius = 7.0F;
        constexpr float StateTextScale = 3.0F;
        const ImVec4 LinkColour = {0.7F, 0.7F, 0.7F, 0.8F};
        const ImVec4 FiredLinkColour = {1.0F, 0.85F, 0.25F, 1.0F};
        const ImVec4 ActiveBorderColour = {0.3F, 1.0F, 0.4F, 1.0F};
        constexpr ImU32 InputPinColour = IM_COL32(90, 190, 255, 255);
        constexpr ImU32 OutputPinColour = IM_COL32(190, 195, 205, 255);

        // Ids the editor tells apart: nodes by state, one input pin per state, and an
        // output pin and a link per transition, each in its own range.
        constexpr std::uintptr_t InputPinIds = 1000;
        constexpr std::uintptr_t OutputPinIds = 2000;
        constexpr std::uintptr_t LinkIds = 3000;

        ed::NodeId nodeId(std::size_t state)
        {
            return ed::NodeId(state + 1);
        }

        ed::PinId inputPinId(std::size_t state)
        {
            return ed::PinId(InputPinIds + state);
        }

        ed::PinId outputPinId(std::size_t transition)
        {
            return ed::PinId(OutputPinIds + transition);
        }

        ed::LinkId linkId(std::size_t transition)
        {
            return ed::LinkId(LinkIds + transition);
        }

        std::optional<std::size_t> transitionIndex(
            std::uintptr_t id,
            std::uintptr_t firstId,
            std::size_t transitionCount)
        {
            if (id < firstId || id - firstId >= transitionCount)
            {
                return std::nullopt;
            }
            return id - firstId;
        }

        void drawPin(ed::PinId id, ed::PinKind kind)
        {
            ed::BeginPin(id, kind);
            const ImVec2 topLeft = ImGui::GetCursorScreenPos();
            ImGui::Dummy({PinHitSize, PinHitSize});
            const ImVec2 center = {topLeft.x + PinHitSize * 0.5F, topLeft.y + PinHitSize * 0.5F};
            if (kind == ed::PinKind::Input)
            {
                ImGui::GetWindowDrawList()->AddCircle(center, PinRadius, InputPinColour, 12, 2.0F);
            }
            else
            {
                ImGui::GetWindowDrawList()->AddCircleFilled(center, PinRadius, OutputPinColour);
            }
            ed::EndPin();
        }

        std::string conditionText(const NpcMachineTransition& transition)
        {
            std::string text;
            for (const auto& [fact, holds] : transition.when)
            {
                if (!text.empty())
                {
                    text += ", ";
                }
                if (!holds)
                {
                    text += '!';
                }
                text += fact;
            }
            if (text.empty())
            {
                text = "always";
            }
            if (transition.after > 0.0F)
            {
                char hold[32]{};
                std::snprintf(hold, sizeof(hold), " after %.2gs", transition.after);
                text += hold;
            }
            return text;
        }

        void layOutInRing(const NpcStateMachine& machine)
        {
            const auto count = static_cast<float>(machine.states.size());
            const float radius = std::max(RingRadiusLeast, RingRadiusPerState * count);
            constexpr float Tau = 6.28318530718F;
            for (std::size_t index = 0; index < machine.states.size(); ++index)
            {
                const float angle = Tau * static_cast<float>(index) / count - Tau * 0.25F;
                ed::SetNodePosition(
                    nodeId(index), {radius * std::cos(angle), radius * std::sin(angle)});
            }
        }

        void drawState(const NpcStateMachine& machine, std::size_t index, bool active)
        {
            const NpcMachineState& state = machine.states[index];
            if (active)
            {
                ed::PushStyleColor(ed::StyleColor_NodeBorder, ActiveBorderColour);
                ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, ActiveBorderWidth);
            }
            ed::BeginNode(nodeId(index));

            const ImVec2 labelTopLeft = ImGui::GetCursorScreenPos();
            ImGui::SetWindowFontScale(StateTextScale);
            ImGui::TextUnformatted(state.name.c_str());
            const std::string activity = nameOf(state.does);
            if (state.name != activity)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", activity.c_str());
            }
            const float labelWidth = ImGui::GetItemRectMax().x - labelTopLeft.x;
            ImGui::SetWindowFontScale(1.0F);

            const auto outputCount = static_cast<std::size_t>(std::count_if(
                machine.transitions.begin(),
                machine.transitions.end(),
                [&state](const NpcMachineTransition& transition)
                { return transition.from == state.name; }));
            const float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
            const float outputsWidth =
                static_cast<float>(outputCount) * PinHitSize +
                static_cast<float>(outputCount > 0 ? outputCount - 1 : 0) * itemSpacing;
            const float pinsWidth =
                PinHitSize + (outputCount > 0 ? itemSpacing + outputsWidth : 0.0F);
            const float contentWidth = std::max(labelWidth, pinsWidth);
            const float pinRowLeft = ImGui::GetCursorPosX();
            drawPin(inputPinId(index), ed::PinKind::Input);

            bool firstOutput = true;
            for (std::size_t transition = 0; transition < machine.transitions.size(); ++transition)
            {
                if (machine.transitions[transition].from != state.name)
                {
                    continue;
                }
                ImGui::SameLine();
                if (firstOutput)
                {
                    ImGui::SetCursorPosX(pinRowLeft + contentWidth - outputsWidth);
                    firstOutput = false;
                }
                drawPin(outputPinId(transition), ed::PinKind::Output);
            }
            ed::EndNode();
            if (active)
            {
                ed::PopStyleVar();
                ed::PopStyleColor();
            }
        }

        void drawTransitions(const NpcStateMachine& machine, std::optional<std::size_t> lastFired)
        {
            for (std::size_t transition = 0; transition < machine.transitions.size(); ++transition)
            {
                const bool fired = lastFired == transition;
                ed::Link(
                    linkId(transition),
                    outputPinId(transition),
                    inputPinId(npcMachineStateNamed(machine, machine.transitions[transition].to)),
                    fired ? FiredLinkColour : LinkColour,
                    fired ? FiredLinkThickness : LinkThickness);
            }
        }

        std::optional<std::size_t> selectedTransition(const NpcStateMachine& machine)
        {
            const ed::PinId hoveredPin = ed::GetHoveredPin();
            if (hoveredPin && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                const std::optional<std::size_t> transition =
                    transitionIndex(hoveredPin.Get(), OutputPinIds, machine.transitions.size());
                if (transition.has_value())
                {
                    ed::SelectLink(linkId(transition.value_or(0)));
                }
            }

            ed::LinkId selectedLink;
            if (ed::GetSelectedLinks(&selectedLink, 1) == 0)
            {
                return std::nullopt;
            }
            return transitionIndex(selectedLink.Get(), LinkIds, machine.transitions.size());
        }

        void drawTransitionDetails(
            const NpcStateMachine& machine,
            std::optional<std::size_t> selected)
        {
            ImGui::Separator();
            if (!selected.has_value())
            {
                ImGui::TextDisabled("Select a transition or its output pin.");
                return;
            }

            const NpcMachineTransition& transition = machine.transitions[selected.value_or(0)];
            ImGui::Text("%s -> %s", transition.from.c_str(), transition.to.c_str());
            ImGui::TextWrapped("Requires: %s", conditionText(transition).c_str());
        }
    }

    MachineGraphEditors::~MachineGraphEditors()
    {
        for (auto& [name, editor] : editors)
        {
            ed::DestroyEditor(editor.context);
        }
    }

    MachineGraphEditor& MachineGraphEditors::editorFor(const std::string& machineName)
    {
        const auto found = editors.find(machineName);
        if (found != editors.end())
        {
            return found->second;
        }

        MachineGraphEditor& editor = editors[machineName];
        editor.settingsFile = "machine_layout_" + machineName + ".json";
        editor.layingOut = !std::filesystem::exists(editor.settingsFile);
        ed::Config config;
        config.SettingsFile = editor.settingsFile.c_str();
        editor.context = ed::CreateEditor(&config);
        return editor;
    }

    void drawMachineGraph(
        MachineGraphEditors& editors,
        const std::optional<MachineDebugInfo>& machine,
        bool locked)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float textPanelWidth =
            DebugTextContentWidth + 2.0F * ImGui::GetStyle().WindowPadding.x;
        const ImVec2 topLeft = {
            viewport->WorkPos.x + FrameProfilePanelWidth + DebugPanelGap,
            viewport->WorkPos.y + DebugPanelGap};
        const ImVec2 size = {
            viewport->WorkSize.x - FrameProfilePanelWidth - textPanelWidth - 2.0F * DebugPanelGap,
            (viewport->WorkSize.y - 2.0F * DebugPanelGap) * WindowHeightFraction};
        if (size.x <= 0.0F || size.y <= 0.0F)
        {
            return;
        }
        ImGui::SetNextWindowPos(topLeft, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        constexpr ImGuiWindowFlags Flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (!ImGui::Begin("Machine", nullptr, Flags))
        {
            ImGui::End();
            return;
        }
        if (!machine.has_value())
        {
            ImGui::TextUnformatted("No NPC with a machine is on screen.");
            ImGui::End();
            return;
        }

        const MachineDebugInfo& shown = machine.value_or(MachineDebugInfo{});
        ImGui::Text(
            "%s of NPC %u%s",
            shown.definition.name.c_str(),
            shown.actor.value,
            locked ? " (locked)" : "");

        MachineGraphEditor& editor = editors.editorFor(shown.definition.name);
        ed::SetCurrentEditor(editor.context);
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float detailsHeight = ImGui::GetTextLineHeightWithSpacing() * 2.0F;
        ed::Begin("##machine", {available.x, std::max(1.0F, available.y - detailsHeight)});
        if (editor.layingOut && editor.framesDrawn == 0)
        {
            layOutInRing(shown.definition);
        }
        for (std::size_t index = 0; index < shown.definition.states.size(); ++index)
        {
            drawState(shown.definition, index, index == shown.active);
        }
        drawTransitions(shown.definition, shown.lastFired);
        if (shown.lastFired.has_value() &&
            (editor.shownActor != shown.actor || editor.shownFired != shown.lastFired))
        {
            ed::Flow(linkId(shown.lastFired.value_or(0)));
        }
        const ImVec2 canvasSize = ed::GetScreenSize();
        ed::End();
        const std::optional<std::size_t> inspected = selectedTransition(shown.definition);
        // Only after End are the states' sizes known, so the view fits them now.
        if (editor.layingOut && editor.framesDrawn > 0 && canvasSize.x == editor.canvasSize.x &&
            canvasSize.y == editor.canvasSize.y)
        {
            ed::NavigateToContent(0.0F);
            editor.layingOut = false;
        }
        editor.canvasSize = canvasSize;
        ed::SetCurrentEditor(nullptr);
        ++editor.framesDrawn;
        editor.shownActor = shown.actor;
        editor.shownFired = shown.lastFired;
        drawTransitionDetails(shown.definition, inspected);
        ImGui::End();
    }
}
