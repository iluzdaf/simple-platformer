#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>

#include <imgui.h>
#include <imgui_node_editor.h>

#include "simple_platformer/actor/actor_id.hpp"

namespace simple_platformer
{
    struct MachineDebugInfo;

    // One machine's node editor and what the machine window keeps for it between
    // frames. The store owns the first two fields: the editor reads its settings file's
    // name through a pointer, so the name lives beside the context. The rest is the
    // window's: when no arrangement was saved, the first frame lays the states out in
    // a ring and the first frame after the canvas has settled at a size fits the view
    // to them, since a size change would undo the fit; and the NPC and transition shown
    // last frame, so a fresh firing flows once.
    struct MachineGraphEditor
    {
        std::string settingsFile;
        ax::NodeEditor::EditorContext* context = nullptr;
        bool layingOut = false;
        int framesDrawn = 0;
        ImVec2 canvasSize = {0.0F, 0.0F};
        std::optional<ActorId> shownActor;
        std::optional<std::size_t> shownFired;
    };

    // The node editors the machine window draws with, one per machine name so each
    // keeps its own arrangement, made the first time a machine is shown and destroyed
    // with the store.
    class MachineGraphEditors
    {
    public:
        MachineGraphEditors() = default;
        MachineGraphEditors(const MachineGraphEditors&) = delete;
        MachineGraphEditors& operator=(const MachineGraphEditors&) = delete;
        MachineGraphEditors(MachineGraphEditors&&) = delete;
        MachineGraphEditors& operator=(MachineGraphEditors&&) = delete;
        ~MachineGraphEditors();

        MachineGraphEditor& editorFor(const std::string& machineName);

    private:
        std::map<std::string, MachineGraphEditor> editors;
    };

    // The machine window, anchored at the top beside the actor text: the followed
    // NPC's machine as a graph, each state a node listing its transitions in priority
    // order, the active state lit and the transition that fired last flowing along its
    // link. States can be dragged about, the canvas panned with the right button and
    // zoomed with the wheel, and F fits it to the graph. The arrangement is kept per
    // machine, in a file beside imgui.ini, so it survives a restart and a change of the
    // NPC followed.
    void drawMachineGraph(
        MachineGraphEditors& editors,
        const std::optional<MachineDebugInfo>& machine);
}
