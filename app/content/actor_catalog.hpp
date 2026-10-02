#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <glm/vec2.hpp>
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

    // Rejects the first projectile sprite that runs past an atlas of this size, naming its
    // field. Actors' own sprites are animation frames, which the animation catalog checks.
    void validateActorAtlasRegions(
        const ActorCatalog& catalog,
        glm::ivec2 atlasSize,
        std::string_view sourceName);

    ActorCatalog loadActorCatalog(
        const std::filesystem::path& path,
        const AnimationCatalog& animations);

    void validateActorCatalog(const ActorCatalog& catalog, const AnimationCatalog& animations);

    const ActorDefinition& actorDefinition(const ActorCatalog& catalog, const std::string& name);
}
