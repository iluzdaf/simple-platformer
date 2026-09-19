#include "actor_catalog.hpp"
#include <algorithm>
#include "game/actor_definition.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/npc/npc.hpp"
#include <exception>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <string_view>
#include <string>

namespace simple_platformer
{
    namespace
    {
        using Json = nlohmann::json;
        void fields(const Json& value, std::initializer_list<std::string_view> allowed)
        {
            if (!value.is_object())
            {
                throw std::invalid_argument("expected an object");
            }
            for (const auto& entry : value.items())
            {
                if (std::find(allowed.begin(), allowed.end(), entry.key()) == allowed.end())
                {
                    throw std::invalid_argument("unknown field '" + entry.key() + "'");
                }
            }
        }
        template <class T> void read(const Json& object, const char* key, T& result)
        {
            if (!object.contains(key))
            {
                return;
            }
            const auto& value = object.at(key);
            if constexpr (std::is_same_v<T, int>)
            {
                if (!value.is_number_integer() || value < std::numeric_limits<int>::min() ||
                    value > std::numeric_limits<int>::max())
                {
                    throw std::invalid_argument(
                        std::string(key) + ": expected an integer in range");
                }
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                if (!value.is_number())
                {
                    throw std::invalid_argument(std::string(key) + ": expected a number");
                }
            }
            else
            {
                if (!value.is_string())
                {
                    throw std::invalid_argument(std::string(key) + ": expected text");
                }
            }
            result = value.get<T>();
        }
        void vector(const Json& object, const char* key, glm::vec2& result)
        {
            if (!object.contains(key))
            {
                return;
            }
            const auto& value = object.at(key);
            if (!value.is_array() || value.size() != 2 || !value[0].is_number() ||
                !value[1].is_number())
            {
                throw std::invalid_argument(std::string(key) + ": expected [x, y]");
            }
            result = {value[0].get<float>(), value[1].get<float>()};
        }
        ActorDefinition parseDefinition(const Json& value)
        {
            fields(
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
                 "ranged"});
            ActorDefinition result;
            vector(value, "bodySize", result.bodySize);
            read(value, "animations", result.animations);
            std::string team = "neutral", facing = "right", anchor = "feet";
            read(value, "team", team);
            read(value, "facing", facing);
            read(value, "spriteAnchor", anchor);
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
                throw std::invalid_argument("unknown team '" + team + "'");
            }
            if (facing == "left")
            {
                result.facing = Facing::Left;
            }
            else if (facing != "right")
            {
                throw std::invalid_argument("unknown facing '" + facing + "'");
            }
            if (anchor == "center")
            {
                result.spriteAnchor = SpriteAnchor::BodyCenter;
            }
            else if (anchor != "feet")
            {
                throw std::invalid_argument("unknown spriteAnchor '" + anchor + "'");
            }
            if (value.contains("health"))
            {
                int health = 0;
                read(value, "health", health);
                result.health = health;
            }
            if (value.contains("inventorySlots"))
            {
                int slots = 0;
                read(value, "inventorySlots", slots);
                result.inventorySlots = slots;
            }
            if (value.contains("platformer"))
            {
                const auto& movement = value.at("platformer");
                fields(
                    movement,
                    {"maximumSpeed",
                     "groundAcceleration",
                     "airAcceleration",
                     "groundDeceleration",
                     "jumpSpeed",
                     "gravity",
                     "jumpReleaseGravity",
                     "maximumFallSpeed",
                     "coyoteTime",
                     "jumpBufferTime"});
                PlatformerMovementConfig config;
                read(movement, "maximumSpeed", config.maximumSpeed);
                read(movement, "groundAcceleration", config.groundAcceleration);
                read(movement, "airAcceleration", config.airAcceleration);
                read(movement, "groundDeceleration", config.groundDeceleration);
                read(movement, "jumpSpeed", config.jumpSpeed);
                read(movement, "gravity", config.gravity);
                read(movement, "jumpReleaseGravity", config.jumpReleaseGravity);
                read(movement, "maximumFallSpeed", config.maximumFallSpeed);
                read(movement, "coyoteTime", config.coyoteTime);
                read(movement, "jumpBufferTime", config.jumpBufferTime);
                result.platformer = config;
            }
            if (value.contains("flying"))
            {
                const auto& movement = value.at("flying");
                fields(movement, {"speed"});
                FlyingMovement config;
                read(movement, "speed", config.speed);
                result.flying = config;
            }
            if (value.contains("senses"))
            {
                const auto& senses = value.at("senses");
                fields(senses, {"noticeDistance", "forgetAfter"});
                NpcSenses config;
                read(senses, "noticeDistance", config.noticeDistance);
                read(senses, "forgetAfter", config.forgetAfter);
                result.senses = config;
            }
            if (value.contains("bite"))
            {
                const auto& bite = value.at("bite");
                fields(
                    bite,
                    {"damage",
                     "hitboxSize",
                     "reach",
                     "windupDuration",
                     "activeDuration",
                     "recoveryDuration"});
                BiteAttack config;
                read(bite, "damage", config.damage);
                vector(bite, "hitboxSize", config.hitboxSize);
                read(bite, "reach", config.reach);
                read(bite, "windupDuration", config.windupDuration);
                read(bite, "activeDuration", config.activeDuration);
                read(bite, "recoveryDuration", config.recoveryDuration);
                result.bite = config;
            }
            if (value.contains("ranged"))
            {
                const auto& ranged = value.at("ranged");
                fields(
                    ranged,
                    {"damage",
                     "projectileSize",
                     "projectileSpeed",
                     "projectileLifetime",
                     "shootDuration",
                     "recoveryDuration",
                     "spritePosition",
                     "spriteSize"});
                RangedWeapon config;
                read(ranged, "damage", config.damage);
                vector(ranged, "projectileSize", config.projectileSize);
                read(ranged, "projectileSpeed", config.projectileSpeed);
                read(ranged, "projectileLifetime", config.projectileLifetime);
                read(ranged, "shootDuration", config.shootDuration);
                read(ranged, "recoveryDuration", config.recoveryDuration);
                vector(ranged, "spritePosition", config.projectileSprite.region.position);
                vector(ranged, "spriteSize", config.projectileSprite.region.size);
                config.projectileSprite.size = config.projectileSprite.region.size;
                result.ranged = config;
            }
            return result;
        }
    }

    ActorCatalog parseActorCatalog(std::string_view text, std::string_view sourceName)
    {
        try
        {
            const auto root = Json::parse(text);
            fields(root, {"player", "actors"});
            ActorCatalog result;
            read(root, "player", result.player);
            const auto& definitions = root.at("actors");
            if (!definitions.is_object() || definitions.empty())
            {
                throw std::invalid_argument("actors: expected a nonempty object");
            }
            for (const auto& entry : definitions.items())
            {
                if (entry.key().empty())
                {
                    throw std::invalid_argument("actor name cannot be empty");
                }
                try
                {
                    result.definitions.emplace(entry.key(), parseDefinition(entry.value()));
                }
                catch (const std::exception& error)
                {
                    throw std::invalid_argument("actors." + entry.key() + ": " + error.what());
                }
            }
            validateActorCatalog(result);
            return result;
        }
        catch (const std::exception& error)
        {
            throw std::invalid_argument(std::string(sourceName) + ": " + error.what());
        }
    }

    void validateActorCatalog(const ActorCatalog& catalog)
    {
        for (const auto& entry : catalog.definitions)
        {
            if (entry.first.empty())
            {
                throw std::invalid_argument("actor name cannot be empty");
            }
            try
            {
                validateActorDefinition(entry.second);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument("actors." + entry.first + ": " + error.what());
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

    ActorCatalog loadActorCatalog(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file)
        {
            throw std::invalid_argument("Could not open actor catalog '" + path.string() + "'");
        }
        const std::string text{
            std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        return parseActorCatalog(text, path.string());
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
