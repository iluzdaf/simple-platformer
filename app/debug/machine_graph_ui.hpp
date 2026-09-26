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

    struct MachineGraphEditor
    {
        // The editor context retains this pointer for its lifetime.
        std::string settingsFile;
        ax::NodeEditor::EditorContext* context = nullptr;

        // Initial fitting waits until node sizes and the canvas have settled.
        bool layingOut = false;
        int framesDrawn = 0;
        ImVec2 canvasSize = {0.0F, 0.0F};

        // Used to animate a firing only when the presented transition changes.
        std::optional<ActorId> shownActor;
        std::optional<std::size_t> shownFired;
    };

    // Owns one node-editor context per machine definition.
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

    void drawMachineGraph(
        MachineGraphEditors& editors,
        const std::optional<MachineDebugInfo>& machine,
        bool locked);
}
