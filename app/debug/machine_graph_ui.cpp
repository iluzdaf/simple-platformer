#include "machine_graph_ui.hpp"

#include "debug_overlay.hpp"
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
        constexpr float WindowWidth = 440.0F;
        constexpr float WindowHeight = 300.0F;
        constexpr float WindowMargin = 8.0F;
        // The overlay's text column, at the right edge, which the window sits beside.
        constexpr float ActorTextWidth = 188.0F;
        constexpr float RingRadiusPerState = 40.0F;
        constexpr float RingRadiusLeast = 120.0F;
        constexpr float LinkThickness = 1.5F;
        constexpr float FiredLinkThickness = 3.0F;
        constexpr float ActiveBorderWidth = 3.0F;
        const ImVec4 LinkColour = {0.7F, 0.7F, 0.7F, 0.8F};
        const ImVec4 FiredLinkColour = {1.0F, 0.85F, 0.25F, 1.0F};
        const ImVec4 ActiveBorderColour = {0.3F, 1.0F, 0.4F, 1.0F};

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

        // The conditions as the data reads them: the facts that must hold, a ! before
        // one that must not, and the hold when there is one.
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

        // The states around a ring in the order the data lists them, the first at the top.
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
            ed::BeginPin(inputPinId(index), ed::PinKind::Input);
            ImGui::TextUnformatted(">");
            ed::EndPin();
            ImGui::SameLine();
            ImGui::TextUnformatted(state.name.c_str());
            // The activity is only worth a mention when the state is not named after it.
            const std::string activity = nameOf(state.does);
            if (state.name != activity)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", activity.c_str());
            }
            for (std::size_t transition = 0; transition < machine.transitions.size(); ++transition)
            {
                if (machine.transitions[transition].from != state.name)
                {
                    continue;
                }
                ImGui::TextUnformatted(conditionText(machine.transitions[transition]).c_str());
                ImGui::SameLine();
                ed::BeginPin(outputPinId(transition), ed::PinKind::Output);
                ImGui::TextUnformatted(">");
                ed::EndPin();
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
        const std::optional<MachineDebugInfo>& machine)
    {
        // Anchored at the top, left of the actor text, without a frame or background,
        // so it sits over the scene like the overlay's other panels. The wheel zooms
        // the graph rather than scrolling the window.
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            {viewport->WorkPos.x + viewport->WorkSize.x - ActorTextWidth - WindowWidth -
                 WindowMargin,
             viewport->WorkPos.y + WindowMargin},
            ImGuiCond_Always);
        ImGui::SetNextWindowSize({WindowWidth, WindowHeight}, ImGuiCond_Always);
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
        ImGui::Text("%s of NPC %u", shown.definition.name.c_str(), shown.actor.value);
        ImGui::SameLine();
        ImGui::TextDisabled("(under the cursor, else nearest the player)");

        MachineGraphEditor& editor = editors.editorFor(shown.definition.name);
        ed::SetCurrentEditor(editor.context);
        ed::Begin("##machine");
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
        ImGui::End();
    }
}
