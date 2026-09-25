#include "actor_catalog.hpp"
#include "machine_catalog.hpp"
#include "content_diagnostics.hpp"
#include "content_json.hpp"
#include "animation_catalog.hpp"
#include "content/actor_definition.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/npc/npc.hpp"
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <string_view>
#include <string>

namespace simple_platformer
{
    namespace
    {
        using Json = nlohmann::json;

        Team jsonTeam(const Json& value, std::string_view sourceName, std::string_view path)
        {
            const std::string team = jsonText(value, sourceName, path);
            if (team == "player")
            {
                return Team::Player;
            }
            if (team == "enemy")
            {
                return Team::Enemy;
            }
            if (team == "neutral")
            {
                return Team::Neutral;
            }
            failJson(
                sourceName,
                path,
                "unknown team '" + team + "'; expected player, enemy, or neutral");
        }

        NpcTactic jsonNpcTactic(
            const Json& value,
            std::string_view sourceName,
            std::string_view path)
        {
            const std::string tactic = jsonText(value, sourceName, path);
            if (tactic == "pursuer")
            {
                return NpcTactic::Pursuer;
            }
            if (tactic == "keepDistance")
            {
                return NpcTactic::KeepDistance;
            }
            failJson(
                sourceName,
                path,
                "unknown tactic '" + tactic + "'; expected pursuer or keepDistance");
        }

        Facing jsonFacing(const Json& value, std::string_view sourceName, std::string_view path)
        {
            const std::string facing = jsonText(value, sourceName, path);
            if (facing == "left")
            {
                return Facing::Left;
            }
            if (facing == "right")
            {
                return Facing::Right;
            }
            failJson(sourceName, path, "unknown facing '" + facing + "'; expected left or right");
        }

        // Each component reader starts from the C++ defaults and takes only the fields the
        // file names, so an empty object means "this component, as configured in code".

        PlatformerMovementConfig jsonPlatformerConfig(
            const Json& value,
            std::string_view sourceName,
            const std::string& path)
        {
            checkJsonFields(
                value,
                {"maximumSpeed",
                 "groundAcceleration",
                 "airAcceleration",
                 "groundDeceleration",
                 "jumpSpeed",
                 "gravity",
                 "jumpReleaseGravity",
                 "maximumFallSpeed",
                 "coyoteDuration",
                 "jumpBufferDuration"},
                sourceName,
                path);
            PlatformerMovementConfig config;
            const auto number = [&](std::string_view key, float& field)
            { readOptionalNumber(value, key, field, sourceName, path); };
            number("maximumSpeed", config.maximumSpeed);
            number("groundAcceleration", config.groundAcceleration);
            number("airAcceleration", config.airAcceleration);
            number("groundDeceleration", config.groundDeceleration);
            number("jumpSpeed", config.jumpSpeed);
            number("gravity", config.gravity);
            number("jumpReleaseGravity", config.jumpReleaseGravity);
            number("maximumFallSpeed", config.maximumFallSpeed);
            number("coyoteDuration", config.coyoteDuration);
            number("jumpBufferDuration", config.jumpBufferDuration);
            return config;
        }

        FlyingMovement jsonFlyingMovement(
            const Json& value,
            std::string_view sourceName,
            const std::string& path)
        {
            checkJsonFields(value, {"speed"}, sourceName, path);
            FlyingMovement config;
            readOptionalNumber(value, "speed", config.speed, sourceName, path);
            return config;
        }

        NpcSenses jsonNpcSenses(
            const Json& value,
            std::string_view sourceName,
            const std::string& path)
        {
            checkJsonFields(
                value,
                {"noticeDistance", "standoffDistance", "targetMemoryDuration", "searchDuration"},
                sourceName,
                path);
            NpcSenses config;
            readOptionalNumber(value, "noticeDistance", config.noticeDistance, sourceName, path);
            readOptionalNumber(
                value, "standoffDistance", config.standoffDistance, sourceName, path);
            readOptionalNumber(
                value, "targetMemoryDuration", config.targetMemoryDuration, sourceName, path);
            readOptionalNumber(value, "searchDuration", config.searchDuration, sourceName, path);
            return config;
        }

        BiteAttack jsonBite(const Json& value, std::string_view sourceName, const std::string& path)
        {
            checkJsonFields(
                value,
                {"damage",
                 "hitboxSize",
                 "reach",
                 "windupDuration",
                 "activeDuration",
                 "recoveryDuration"},
                sourceName,
                path);
            BiteAttack config;
            const auto number = [&](std::string_view key, float& field)
            { readOptionalNumber(value, key, field, sourceName, path); };
            readOptionalInteger(value, "damage", config.damage, sourceName, path);
            readOptionalVector(value, "hitboxSize", config.hitboxSize, sourceName, path);
            number("reach", config.reach);
            number("windupDuration", config.windupDuration);
            number("activeDuration", config.activeDuration);
            number("recoveryDuration", config.recoveryDuration);
            return config;
        }

        RangedWeapon jsonRangedWeapon(
            const Json& value,
            std::string_view sourceName,
            const std::string& path)
        {
            checkJsonFields(
                value,
                {"damage",
                 "projectileSize",
                 "projectileSpeed",
                 "projectileLifetime",
                 "shootDuration",
                 "recoveryDuration",
                 "breaksTiles",
                 "sprite"},
                sourceName,
                path);
            RangedWeapon config;
            const auto number = [&](std::string_view key, float& field)
            { readOptionalNumber(value, key, field, sourceName, path); };
            readOptionalInteger(value, "damage", config.damage, sourceName, path);
            readOptionalVector(value, "projectileSize", config.projectileSize, sourceName, path);
            number("projectileSpeed", config.projectileSpeed);
            number("projectileLifetime", config.projectileLifetime);
            number("shootDuration", config.shootDuration);
            number("recoveryDuration", config.recoveryDuration);
            readOptionalBoolean(value, "breaksTiles", config.breaksTiles, sourceName, path);
            readOptionalSprite(value, "sprite", config.projectileSprite, sourceName, path);
            return config;
        }

        // The optional fields, in the readOptional shape: a missing key leaves the field
        // alone, a present one is converted by the matching json reader.

        void readOptionalTeam(
            const Json& object,
            std::string_view key,
            Team& result,
            std::string_view sourceName,
            const std::string& path)
        {
            if (const Json* found = optionalJsonMember(object, key, sourceName, path))
            {
                result = jsonTeam(*found, sourceName, fieldPath(path, key));
            }
        }

        void readOptionalNpcTactic(
            const Json& object,
            std::string_view key,
            NpcTactic& result,
            std::string_view sourceName,
            const std::string& path)
        {
            if (const Json* found = optionalJsonMember(object, key, sourceName, path))
            {
                result = jsonNpcTactic(*found, sourceName, fieldPath(path, key));
            }
        }

        void readOptionalFacing(
            const Json& object,
            std::string_view key,
            Facing& result,
            std::string_view sourceName,
            const std::string& path)
        {
            if (const Json* found = optionalJsonMember(object, key, sourceName, path))
            {
                result = jsonFacing(*found, sourceName, fieldPath(path, key));
            }
        }

        void readOptionalPlatformerConfig(
            const Json& object,
            std::string_view key,
            std::optional<PlatformerMovementConfig>& result,
            std::string_view sourceName,
            const std::string& path)
        {
            if (const Json* found = optionalJsonMember(object, key, sourceName, path))
            {
                result = jsonPlatformerConfig(*found, sourceName, fieldPath(path, key));
            }
        }

        void readOptionalFlyingMovement(
            const Json& object,
            std::string_view key,
            std::optional<FlyingMovement>& result,
            std::string_view sourceName,
            const std::string& path)
        {
            if (const Json* found = optionalJsonMember(object, key, sourceName, path))
            {
                result = jsonFlyingMovement(*found, sourceName, fieldPath(path, key));
            }
        }

        void readOptionalNpcSenses(
            const Json& object,
            std::string_view key,
            std::optional<NpcSenses>& result,
            std::string_view sourceName,
            const std::string& path)
        {
            if (const Json* found = optionalJsonMember(object, key, sourceName, path))
            {
                result = jsonNpcSenses(*found, sourceName, fieldPath(path, key));
            }
        }

        void readOptionalBite(
            const Json& object,
            std::string_view key,
            std::optional<BiteAttack>& result,
            std::string_view sourceName,
            const std::string& path)
        {
            if (const Json* found = optionalJsonMember(object, key, sourceName, path))
            {
                result = jsonBite(*found, sourceName, fieldPath(path, key));
            }
        }

        void readOptionalRangedWeapon(
            const Json& object,
            std::string_view key,
            std::optional<RangedWeapon>& result,
            std::string_view sourceName,
            const std::string& path)
        {
            if (const Json* found = optionalJsonMember(object, key, sourceName, path))
            {
                result = jsonRangedWeapon(*found, sourceName, fieldPath(path, key));
            }
        }

        ActorDefinition jsonActorDefinition(
            const Json& value,
            std::string_view sourceName,
            const std::string& path)
        {
            checkJsonFields(
                value,
                {"bodySize",
                 "team",
                 "facing",
                 "spriteAnchor",
                 "animations",
                 "health",
                 "inventorySlots",
                 "platformer",
                 "flying",
                 "senses",
                 "tactic",
                 "machine",
                 "bite",
                 "ranged"},
                sourceName,
                path);
            ActorDefinition result;
            result.bodySize = readVector(value, "bodySize", sourceName, path);
            readOptionalText(value, "animations", result.animations, sourceName, path);
            readOptionalSpriteAnchor(value, "spriteAnchor", result.spriteAnchor, sourceName, path);
            readOptionalTeam(value, "team", result.team, sourceName, path);
            readOptionalFacing(value, "facing", result.facing, sourceName, path);
            readOptionalInteger(value, "health", result.health, sourceName, path);
            readOptionalInteger(value, "inventorySlots", result.inventorySlots, sourceName, path);
            readOptionalPlatformerConfig(value, "platformer", result.platformer, sourceName, path);
            readOptionalFlyingMovement(value, "flying", result.flying, sourceName, path);
            readOptionalNpcSenses(value, "senses", result.senses, sourceName, path);
            readOptionalNpcTactic(value, "tactic", result.tactic, sourceName, path);
            readOptionalText(value, "machine", result.machine, sourceName, path);
            readOptionalBite(value, "bite", result.bite, sourceName, path);
            readOptionalRangedWeapon(value, "ranged", result.ranged, sourceName, path);
            return result;
        }
    }

    ActorCatalog parseActorCatalog(
        std::string_view text,
        std::string_view sourceName,
        const AnimationCatalog& animations,
        const MachineCatalog& machines)
    {
        const auto root = parseContentRoot(text, sourceName);
        checkJsonFields(root, {"player", "actors"}, sourceName, "root");
        ActorCatalog result;
        result.player = readText(root, "player", sourceName, "root");
        const auto& definitions = requiredJsonMember(root, "actors", sourceName, "root");
        checkJsonObject(definitions, sourceName, "actors");
        if (definitions.empty())
        {
            failJson(sourceName, "actors", "expected a nonempty object");
        }
        for (const auto& entry : definitions.items())
        {
            if (entry.key().empty())
            {
                failJson(sourceName, "actors", "actor name cannot be empty");
            }
            result.definitions.emplace(
                entry.key(),
                jsonActorDefinition(entry.value(), sourceName, fieldPath("actors", entry.key())));
        }
        validateInFile(sourceName, [&] { validateActorCatalog(result, animations, machines); });
        return result;
    }

    void validateActorCatalog(
        const ActorCatalog& catalog,
        const AnimationCatalog& animations,
        const MachineCatalog& machines)
    {
        for (const auto& entry : catalog.definitions)
        {
            if (entry.first.empty())
            {
                throw std::invalid_argument("actor name cannot be empty");
            }
            try
            {
                validateActorDefinition(entry.second, animations, machines);
            }
            catch (const std::invalid_argument& error)
            {
                failJson({}, fieldPath("actors", entry.first), error.what());
            }
        }
        const auto& player = actorDefinition(catalog, catalog.player);
        if (player.senses)
        {
            throw std::invalid_argument("player definition must not enable NPC sensing");
        }
        if (!player.health || !player.inventorySlots)
        {
            throw std::invalid_argument(
                "player definition requires health and inventorySlots for the game HUD");
        }
    }

    ActorCatalog loadActorCatalog(
        const std::filesystem::path& path,
        const AnimationCatalog& animations,
        const MachineCatalog& machines)
    {
        return parseActorCatalog(loadContentText(path), path.string(), animations, machines);
    }

    const ActorDefinition& actorDefinition(const ActorCatalog& catalog, const std::string& name)
    {
        const auto found = catalog.definitions.find(name);
        if (found == catalog.definitions.end())
        {
            throw std::invalid_argument("unknown actor definition '" + name + "'");
        }
        return found->second;
    }
}
