#include "content_json.hpp"
#include "content_diagnostics.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <nlohmann/json.hpp>
#include <glm/vec2.hpp>
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    void checkJsonObject(
        const nlohmann::json& value,
        std::string_view sourceName,
        std::string_view path)
    {
        if (!value.is_object())
        {
            failJson(sourceName, path, "expected an object");
        }
    }

    void checkJsonFields(
        const nlohmann::json& value,
        const std::vector<std::string_view>& allowed,
        std::string_view sourceName,
        std::string_view path)
    {
        checkJsonObject(value, sourceName, path);
        for (const auto& entry : value.items())
        {
            if (std::find(allowed.begin(), allowed.end(), entry.key()) == allowed.end())
            {
                failJson(sourceName, path, "unknown field '" + entry.key() + "'");
            }
        }
    }

    const nlohmann::json* optionalJsonMember(
        const nlohmann::json& object,
        std::string_view key,
        std::string_view sourceName,
        std::string_view path)
    {
        checkJsonObject(object, sourceName, path);
        const auto found = object.find(std::string(key));
        return found == object.end() ? nullptr : &*found;
    }

    const nlohmann::json& requiredJsonMember(
        const nlohmann::json& object,
        std::string_view key,
        std::string_view sourceName,
        std::string_view path)
    {
        const nlohmann::json* found = optionalJsonMember(object, key, sourceName, path);
        if (found == nullptr)
        {
            failJson(sourceName, path, "missing '" + std::string(key) + "'");
        }
        return *found;
    }

    int jsonInteger(const nlohmann::json& value, std::string_view sourceName, std::string_view path)
    {
        if (!value.is_number_integer())
        {
            failJson(sourceName, path, "expected an integer");
        }
        if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
        {
            failJson(sourceName, path, "integer is outside the supported range");
        }
        return value.get<int>();
    }

    float jsonNumber(
        const nlohmann::json& value,
        std::string_view sourceName,
        std::string_view path)
    {
        if (!value.is_number())
        {
            failJson(sourceName, path, "expected a number");
        }
        const float result = value.get<float>();
        if (!std::isfinite(result))
        {
            failJson(sourceName, path, "number must be finite");
        }
        return result;
    }

    bool jsonBoolean(
        const nlohmann::json& value,
        std::string_view sourceName,
        std::string_view path)
    {
        if (!value.is_boolean())
        {
            failJson(sourceName, path, "expected true or false");
        }
        return value.get<bool>();
    }

    std::string jsonText(
        const nlohmann::json& value,
        std::string_view sourceName,
        std::string_view path)
    {
        if (!value.is_string())
        {
            failJson(sourceName, path, "expected text");
        }
        return value.get<std::string>();
    }

    std::string jsonName(
        const nlohmann::json& value,
        std::string_view description,
        std::string_view sourceName,
        std::string_view path)
    {
        std::string name = jsonText(value, sourceName, path);
        if (name.empty())
        {
            failJson(sourceName, path, std::string(description) + " cannot be empty");
        }
        return name;
    }

    std::string readName(
        const nlohmann::json& object,
        std::string_view key,
        std::string_view description,
        std::string_view sourceName,
        std::string_view path)
    {
        return jsonName(
            requiredJsonMember(object, key, sourceName, path),
            description,
            sourceName,
            fieldPath(path, key));
    }

    void checkJsonPair(
        const nlohmann::json& value,
        std::string_view shape,
        std::string_view sourceName,
        std::string_view path)
    {
        if (!value.is_array() || value.size() != 2)
        {
            failJson(sourceName, path, std::string("expected ") + std::string(shape));
        }
    }

    glm::vec2 jsonVector(
        const nlohmann::json& value,
        std::string_view sourceName,
        std::string_view path)
    {
        checkJsonPair(value, "[x, y]", sourceName, path);
        return {
            jsonNumber(value[0], sourceName, indexPath(path, 0)),
            jsonNumber(value[1], sourceName, indexPath(path, 1))};
    }

    int readInteger(
        const nlohmann::json& object,
        std::string_view key,
        std::string_view sourceName,
        std::string_view path)
    {
        return jsonInteger(
            requiredJsonMember(object, key, sourceName, path), sourceName, fieldPath(path, key));
    }

    void readOptionalInteger(
        const nlohmann::json& object,
        std::string_view key,
        int& result,
        std::string_view sourceName,
        std::string_view path)
    {
        if (const nlohmann::json* found = optionalJsonMember(object, key, sourceName, path))
        {
            result = jsonInteger(*found, sourceName, fieldPath(path, key));
        }
    }

    float readNumber(
        const nlohmann::json& object,
        std::string_view key,
        std::string_view sourceName,
        std::string_view path)
    {
        return jsonNumber(
            requiredJsonMember(object, key, sourceName, path), sourceName, fieldPath(path, key));
    }

    void readOptionalNumber(
        const nlohmann::json& object,
        std::string_view key,
        float& result,
        std::string_view sourceName,
        std::string_view path)
    {
        if (const nlohmann::json* found = optionalJsonMember(object, key, sourceName, path))
        {
            result = jsonNumber(*found, sourceName, fieldPath(path, key));
        }
    }

    bool readBoolean(
        const nlohmann::json& object,
        std::string_view key,
        std::string_view sourceName,
        std::string_view path)
    {
        return jsonBoolean(
            requiredJsonMember(object, key, sourceName, path), sourceName, fieldPath(path, key));
    }

    void readOptionalBoolean(
        const nlohmann::json& object,
        std::string_view key,
        bool& result,
        std::string_view sourceName,
        std::string_view path)
    {
        if (const nlohmann::json* found = optionalJsonMember(object, key, sourceName, path))
        {
            result = jsonBoolean(*found, sourceName, fieldPath(path, key));
        }
    }

    std::string readText(
        const nlohmann::json& object,
        std::string_view key,
        std::string_view sourceName,
        std::string_view path)
    {
        return jsonText(
            requiredJsonMember(object, key, sourceName, path), sourceName, fieldPath(path, key));
    }

    void readOptionalText(
        const nlohmann::json& object,
        std::string_view key,
        std::string& result,
        std::string_view sourceName,
        std::string_view path)
    {
        if (const nlohmann::json* found = optionalJsonMember(object, key, sourceName, path))
        {
            result = jsonText(*found, sourceName, fieldPath(path, key));
        }
    }

    glm::vec2 readVector(
        const nlohmann::json& object,
        std::string_view key,
        std::string_view sourceName,
        std::string_view path)
    {
        return jsonVector(
            requiredJsonMember(object, key, sourceName, path), sourceName, fieldPath(path, key));
    }

    void readOptionalVector(
        const nlohmann::json& object,
        std::string_view key,
        glm::vec2& result,
        std::string_view sourceName,
        std::string_view path)
    {
        if (const nlohmann::json* found = optionalJsonMember(object, key, sourceName, path))
        {
            result = jsonVector(*found, sourceName, fieldPath(path, key));
        }
    }

    SpriteRegion jsonSpriteRegion(
        const nlohmann::json& value,
        std::string_view sourceName,
        std::string_view path)
    {
        SpriteRegion region;
        region.position = readVector(value, "position", sourceName, path);
        region.size = readVector(value, "size", sourceName, path);
        return region;
    }

    void readOptionalSpriteAnchor(
        const nlohmann::json& object,
        std::string_view key,
        SpriteAnchor& result,
        std::string_view sourceName,
        std::string_view path)
    {
        std::string anchor;
        readOptionalText(object, key, anchor, sourceName, path);
        if (anchor.empty())
        {
            return;
        }
        if (anchor == "feet")
        {
            result = SpriteAnchor::BodyFeet;
        }
        else if (anchor == "center")
        {
            result = SpriteAnchor::BodyCenter;
        }
        else
        {
            failJson(
                sourceName,
                fieldPath(path, key),
                "unknown sprite anchor '" + anchor + "'; expected feet or center");
        }
    }

    Sprite jsonSprite(
        const nlohmann::json& value,
        std::string_view sourceName,
        std::string_view path)
    {
        checkJsonFields(value, {"position", "size", "displaySize", "anchor"}, sourceName, path);
        Sprite sprite;
        sprite.region = jsonSpriteRegion(value, sourceName, path);
        sprite.size = sprite.region.size;
        readOptionalVector(value, "displaySize", sprite.size, sourceName, path);
        readOptionalSpriteAnchor(value, "anchor", sprite.anchor, sourceName, path);
        return sprite;
    }

    void readOptionalInteger(
        const nlohmann::json& object,
        std::string_view key,
        std::optional<int>& result,
        std::string_view sourceName,
        std::string_view path)
    {
        if (const nlohmann::json* found = optionalJsonMember(object, key, sourceName, path))
        {
            result = jsonInteger(*found, sourceName, fieldPath(path, key));
        }
    }

    void readOptionalSprite(
        const nlohmann::json& object,
        std::string_view key,
        Sprite& result,
        std::string_view sourceName,
        std::string_view path)
    {
        if (const nlohmann::json* found = optionalJsonMember(object, key, sourceName, path))
        {
            result = jsonSprite(*found, sourceName, fieldPath(path, key));
        }
    }

    void readOptionalSprite(
        const nlohmann::json& object,
        std::string_view key,
        std::optional<Sprite>& result,
        std::string_view sourceName,
        std::string_view path)
    {
        if (const nlohmann::json* found = optionalJsonMember(object, key, sourceName, path))
        {
            result = jsonSprite(*found, sourceName, fieldPath(path, key));
        }
    }

    std::string loadContentText(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file)
        {
            throw std::invalid_argument("Could not open content file '" + path.string() + "'");
        }
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    nlohmann::json parseContentRoot(std::string_view text, std::string_view sourceName)
    {
        try
        {
            return nlohmann::json::parse(text.begin(), text.end());
        }
        catch (const nlohmann::json::parse_error& error)
        {
            // JSON reports a one-based byte position, including one past the end at EOF.
            std::size_t line = 1;
            std::size_t column = 1;
            for (std::size_t index = 0; index < text.size() && index + 1 < error.byte; ++index)
            {
                if (text[index] == '\n')
                {
                    ++line;
                    column = 1;
                }
                else
                {
                    ++column;
                }
            }
            failJson(
                sourceName,
                "line " + std::to_string(line) + ", column " + std::to_string(column),
                std::string("invalid JSON: ") + error.what());
        }
    }
}
