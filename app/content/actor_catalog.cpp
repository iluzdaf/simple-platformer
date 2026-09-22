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
            readOptionalVector(value, "bodySize", result.bodySize, sourceName, path);
            readOptionalText(value, "animations", result.animations, sourceName, path);
            std::string team = "neutral", facing = "right";
            readOptionalText(value, "team", team, sourceName, path);
            readOptionalText(value, "facing", facing, sourceName, path);
            readOptionalSpriteAnchor(value, "spriteAnchor", result.spriteAnchor, sourceName, path);
            if (team == "player")
            {
                result.team = Team::Player;
            }
            else if (team == "enemy")
            {
                result.team = Team::Enemy;
            }
            else if (team != "neutral")
            {
                failJson(
                    sourceName,
                    fieldPath(path, "team"),
                    "unknown team '" + team + "'; expected player, enemy, or neutral");
            }
            if (facing == "left")
            {
                result.facing = Facing::Left;
            }
            else if (facing != "right")
            {
                failJson(
                    sourceName,
                    fieldPath(path, "facing"),
                    "unknown facing '" + facing + "'; expected left or right");
            }
            if (value.contains("health"))
            {
                result.health = readInteger(value, "health", sourceName, path);
            }
            if (value.contains("inventorySlots"))
            {
                result.inventorySlots = readInteger(value, "inventorySlots", sourceName, path);
            }
            if (value.contains("platformer"))
            {
                const auto& movement = requiredJsonMember(value, "platformer", sourceName, path);
                const std::string platformerPath = fieldPath(path, "platformer");
                checkJsonFields(
                    movement,
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
                    platformerPath);
                PlatformerMovementConfig config;
                readOptionalNumber(
                    movement, "maximumSpeed", config.maximumSpeed, sourceName, platformerPath);
                readOptionalNumber(
                    movement,
                    "groundAcceleration",
                    config.groundAcceleration,
                    sourceName,
                    platformerPath);
                readOptionalNumber(
                    movement,
                    "airAcceleration",
                    config.airAcceleration,
                    sourceName,
                    platformerPath);
                readOptionalNumber(
                    movement,
                    "groundDeceleration",
                    config.groundDeceleration,
                    sourceName,
                    platformerPath);
                readOptionalNumber(
                    movement, "jumpSpeed", config.jumpSpeed, sourceName, platformerPath);
                readOptionalNumber(movement, "gravity", config.gravity, sourceName, platformerPath);
                readOptionalNumber(
                    movement,
                    "jumpReleaseGravity",
                    config.jumpReleaseGravity,
                    sourceName,
                    platformerPath);
                readOptionalNumber(
                    movement,
                    "maximumFallSpeed",
                    config.maximumFallSpeed,
                    sourceName,
                    platformerPath);
                readOptionalNumber(
                    movement, "coyoteDuration", config.coyoteDuration, sourceName, platformerPath);
                readOptionalNumber(
                    movement,
                    "jumpBufferDuration",
                    config.jumpBufferDuration,
                    sourceName,
                    platformerPath);
                result.platformer = config;
            }
            if (value.contains("flying"))
            {
                const auto& movement = requiredJsonMember(value, "flying", sourceName, path);
                const std::string flyingPath = fieldPath(path, "flying");
                checkJsonFields(movement, {"speed"}, sourceName, flyingPath);
                FlyingMovement config;
                readOptionalNumber(movement, "speed", config.speed, sourceName, flyingPath);
                result.flying = config;
            }
            if (value.contains("senses"))
            {
                const auto& senses = requiredJsonMember(value, "senses", sourceName, path);
                const std::string sensesPath = fieldPath(path, "senses");
                checkJsonFields(
                    senses, {"noticeDistance", "targetMemoryDuration"}, sourceName, sensesPath);
                NpcSenses config;
                readOptionalNumber(
                    senses, "noticeDistance", config.noticeDistance, sourceName, sensesPath);
                readOptionalNumber(
                    senses,
                    "targetMemoryDuration",
                    config.targetMemoryDuration,
                    sourceName,
                    sensesPath);
                result.senses = config;
            }
            if (value.contains("bite"))
            {
                const auto& bite = requiredJsonMember(value, "bite", sourceName, path);
                const std::string bitePath = fieldPath(path, "bite");
                checkJsonFields(
                    bite,
                    {"damage",
                     "hitboxSize",
                     "reach",
                     "windupDuration",
                     "activeDuration",
                     "recoveryDuration"},
                    sourceName,
                    bitePath);
                BiteAttack config;
                readOptionalInteger(bite, "damage", config.damage, sourceName, bitePath);
                readOptionalVector(bite, "hitboxSize", config.hitboxSize, sourceName, bitePath);
                readOptionalNumber(bite, "reach", config.reach, sourceName, bitePath);
                readOptionalNumber(
                    bite, "windupDuration", config.windupDuration, sourceName, bitePath);
                readOptionalNumber(
                    bite, "activeDuration", config.activeDuration, sourceName, bitePath);
                readOptionalNumber(
                    bite, "recoveryDuration", config.recoveryDuration, sourceName, bitePath);
                result.bite = config;
            }
            if (value.contains("ranged"))
            {
                const auto& ranged = requiredJsonMember(value, "ranged", sourceName, path);
                const std::string rangedPath = fieldPath(path, "ranged");
                checkJsonFields(
                    ranged,
                    {"damage",
                     "projectileSize",
                     "projectileSpeed",
                     "projectileLifetime",
                     "shootDuration",
                     "recoveryDuration",
                     "breaksTiles",
                     "sprite"},
                    sourceName,
                    rangedPath);
                RangedWeapon config;
                readOptionalInteger(ranged, "damage", config.damage, sourceName, rangedPath);
                readOptionalVector(
                    ranged, "projectileSize", config.projectileSize, sourceName, rangedPath);
                readOptionalNumber(
                    ranged, "projectileSpeed", config.projectileSpeed, sourceName, rangedPath);
                readOptionalNumber(
                    ranged,
                    "projectileLifetime",
                    config.projectileLifetime,
                    sourceName,
                    rangedPath);
                readOptionalNumber(
                    ranged, "shootDuration", config.shootDuration, sourceName, rangedPath);
                readOptionalNumber(
                    ranged, "recoveryDuration", config.recoveryDuration, sourceName, rangedPath);
                readOptionalBoolean(
                    ranged, "breaksTiles", config.breaksTiles, sourceName, rangedPath);
                if (ranged.contains("sprite"))
                {
                    config.projectileSprite = jsonSprite(
                        requiredJsonMember(ranged, "sprite", sourceName, rangedPath),
                        sourceName,
                        fieldPath(rangedPath, "sprite"));
                }
                result.ranged = config;
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
