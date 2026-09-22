#include "actor_catalog.hpp"
#include "content_diagnostics.hpp"
#include "content_json.hpp"
#include "animation_catalog.hpp"
#include "content/actor_definition.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/npc/npc.hpp"
#include <filesystem>
#include <initializer_list>
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
            checkJsonFields(value, {"noticeDistance", "targetMemoryDuration"}, sourceName, path);
            NpcSenses config;
            readOptionalNumber(value, "noticeDistance", config.noticeDistance, sourceName, path);
            readOptionalNumber(
                value, "targetMemoryDuration", config.targetMemoryDuration, sourceName, path);
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
            if (const Json* sprite = optionalJsonMember(value, "sprite"))
            {
                config.projectileSprite =
                    jsonSprite(*sprite, sourceName, fieldPath(path, "sprite"));
            }
            return config;
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
                 "bite",
                 "ranged"},
                sourceName,
                path);
            ActorDefinition result;
            result.bodySize = readVector(value, "bodySize", sourceName, path);
            readOptionalText(value, "animations", result.animations, sourceName, path);
            readOptionalSpriteAnchor(value, "spriteAnchor", result.spriteAnchor, sourceName, path);
            if (const Json* team = optionalJsonMember(value, "team"))
            {
                result.team = jsonTeam(*team, sourceName, fieldPath(path, "team"));
            }
            if (const Json* facing = optionalJsonMember(value, "facing"))
            {
                result.facing = jsonFacing(*facing, sourceName, fieldPath(path, "facing"));
            }
            if (const Json* health = optionalJsonMember(value, "health"))
            {
                result.health = jsonInteger(*health, sourceName, fieldPath(path, "health"));
            }
            if (const Json* slots = optionalJsonMember(value, "inventorySlots"))
            {
                result.inventorySlots =
                    jsonInteger(*slots, sourceName, fieldPath(path, "inventorySlots"));
            }
            if (const Json* platformer = optionalJsonMember(value, "platformer"))
            {
                result.platformer =
                    jsonPlatformerConfig(*platformer, sourceName, fieldPath(path, "platformer"));
            }
            if (const Json* flying = optionalJsonMember(value, "flying"))
            {
                result.flying = jsonFlyingMovement(*flying, sourceName, fieldPath(path, "flying"));
            }
            if (const Json* senses = optionalJsonMember(value, "senses"))
            {
                result.senses = jsonNpcSenses(*senses, sourceName, fieldPath(path, "senses"));
            }
            if (const Json* bite = optionalJsonMember(value, "bite"))
            {
                result.bite = jsonBite(*bite, sourceName, fieldPath(path, "bite"));
            }
            if (const Json* ranged = optionalJsonMember(value, "ranged"))
            {
                result.ranged = jsonRangedWeapon(*ranged, sourceName, fieldPath(path, "ranged"));
            }
            return result;
        }
    }

    ActorCatalog parseActorCatalog(
        std::string_view text,
        std::string_view sourceName,
        const AnimationCatalog& animations)
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
        validateInFile(sourceName, [&] { validateActorCatalog(result, animations); });
        return result;
    }

    void validateActorCatalog(const ActorCatalog& catalog, const AnimationCatalog& animations)
    {
        for (const auto& entry : catalog.definitions)
        {
            if (entry.first.empty())
            {
                throw std::invalid_argument("actor name cannot be empty");
            }
            try
            {
                validateActorDefinition(entry.second, animations);
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
        const AnimationCatalog& animations)
    {
        return parseActorCatalog(loadContentText(path), path.string(), animations);
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
