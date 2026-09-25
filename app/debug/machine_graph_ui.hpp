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

    // The machine window, anchored at the top beside the actor text: the
    // followed NPC's machine as a graph, each state a node
    // listing its transitions in priority order, the active state lit and the transition
    // that fired last flowing along its link. States can be dragged about, the canvas
    // panned with the right button and zoomed with the wheel, and F fits it to the
    // graph. The arrangement is kept per machine, in a file beside imgui.ini, so it
    // survives a restart and a change of the NPC followed.
    class MachineGraph
    {
    public:
        MachineGraph() = default;
        MachineGraph(const MachineGraph&) = delete;
        MachineGraph& operator=(const MachineGraph&) = delete;
        MachineGraph(MachineGraph&&) = delete;
        MachineGraph& operator=(MachineGraph&&) = delete;
        ~MachineGraph();

        void draw(const std::optional<MachineDebugInfo>& machine);

    private:
        // One editor per machine name, so each keeps its own arrangement. The editor
        // reads the settings file's name through a pointer, so the name lives here.
        struct Editor
        {
            std::string settingsFile;
            ax::NodeEditor::EditorContext* context = nullptr;
            // Set when no arrangement was saved: the first frame lays the states out in
            // a ring, and the first frame after the canvas has settled at a size fits
            // the view to them, since a size change would undo the fit.
            bool layingOut = false;
            int framesDrawn = 0;
            ImVec2 canvasSize = {0.0F, 0.0F};
        };

        Editor& editorFor(const std::string& machineName);

        std::map<std::string, Editor> editors;
        // The NPC and transition shown last frame, so a fresh firing flows once.
        std::optional<ActorId> shownActor;
        std::optional<std::size_t> shownFired;
    };
}
