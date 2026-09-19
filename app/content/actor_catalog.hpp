#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include "actor_definition.hpp"
#include "animation_catalog.hpp"

namespace simple_platformer
{
    struct ActorCatalog
    {
        std::string player;
        std::map<std::string, ActorDefinition> definitions;
    };

    ActorCatalog parseActorCatalog(
        std::string_view text,
        std::string_view sourceName,
        const AnimationCatalog& animations);

    ActorCatalog loadActorCatalog(
        const std::filesystem::path& path,
        const AnimationCatalog& animations);

    void validateActorCatalog(const ActorCatalog& catalog, const AnimationCatalog& animations);

    const ActorDefinition& actorDefinition(const ActorCatalog& catalog, const std::string& name);
}
