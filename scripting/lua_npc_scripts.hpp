#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "simple_platformer/actor/actor_id.hpp"
#include "simple_platformer/npc/npc_activity.hpp"
#include "simple_platformer/npc/npc_activity_script.hpp"

namespace simple_platformer
{
    struct LuaScriptDiagnostic
    {
        std::string source;
        std::string script;
        std::string activity;
        std::string hook;
        std::optional<ActorId> actor;
        std::string message;
    };

    // Owns the Lua VM, script environments and per-actor activity memory. Lua types stay
    // private to the implementation.
    class LuaNpcScripts final : public NpcActivityScripts
    {
    public:
        LuaNpcScripts();
        ~LuaNpcScripts() override;
        LuaNpcScripts(LuaNpcScripts&& other) noexcept;
        LuaNpcScripts& operator=(LuaNpcScripts&& other) noexcept;
        LuaNpcScripts(const LuaNpcScripts&) = delete;
        LuaNpcScripts& operator=(const LuaNpcScripts&) = delete;

        void loadScript(const std::string& script, const std::filesystem::path& path);
        void loadScriptText(
            const std::string& script,
            std::string_view source,
            std::string sourceName = "Lua script");
        bool hasScript(std::string_view script) const;
        bool hasActivity(const LuaNpcActivity& activity) const;

        void enter(
            ActorId actor,
            const LuaNpcActivity& activity,
            const NpcActivitySnapshot& snapshot) override;
        NpcActivityCommand update(
            ActorId actor,
            const LuaNpcActivity& activity,
            const NpcActivitySnapshot& snapshot,
            float deltaTime) override;
        void exit(
            ActorId actor,
            const LuaNpcActivity& activity,
            const NpcActivitySnapshot& snapshot) override;
        void forget(ActorId actor) override;

        const std::vector<LuaScriptDiagnostic>& diagnostics() const;
        void clearDiagnostics();

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation;
    };
}
