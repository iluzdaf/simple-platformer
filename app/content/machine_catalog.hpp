#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include "simple_platformer/npc/npc_state_machine.hpp"

namespace simple_platformer
{
    // Data-driven NPC state machines by name. An actor definition names one to run
    // instead of its brain's tactic.
    using MachineCatalog = std::map<std::string, NpcStateMachine>;

    void validateMachineCatalog(const MachineCatalog& catalog);
    MachineCatalog parseMachineCatalog(std::string_view text, std::string_view sourceName);
    MachineCatalog loadMachineCatalog(const std::filesystem::path& path);
    const NpcStateMachine& npcStateMachine(const MachineCatalog& catalog, const std::string& name);
}
