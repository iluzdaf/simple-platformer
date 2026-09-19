#include "content_json.hpp"
#include "simple_platformer/render/sprite.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string_view>
#include <string>

namespace simple_platformer
{
    void checkJsonFields(
        const nlohmann::json& value,
        std::initializer_list<std::string_view> allowed)
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
    int jsonInteger(const nlohmann::json& value)
    {
        if (!value.is_number_integer() || value < std::numeric_limits<int>::min() ||
            value > std::numeric_limits<int>::max())
        {
            throw std::invalid_argument("expected an integer in range");
        }
        return value.get<int>();
    }
    glm::vec2 jsonVector(const nlohmann::json& value)
    {
        if (!value.is_array() || value.size() != 2 || !value[0].is_number() ||
            !value[1].is_number())
        {
            throw std::invalid_argument("expected [x, y]");
        }
        return {value[0].get<float>(), value[1].get<float>()};
    }
    Sprite jsonSprite(const nlohmann::json& value)
    {
        checkJsonFields(value, {"position", "size", "displaySize", "anchor"});
        Sprite sprite;
        sprite.region.position = jsonVector(value.at("position"));
        sprite.region.size = jsonVector(value.at("size"));
        sprite.size = value.contains("displaySize") ? jsonVector(value.at("displaySize"))
                                                    : sprite.region.size;
        if (value.contains("anchor"))
        {
            const auto anchor = value.at("anchor").get<std::string>();
            if (anchor == "center")
            {
                sprite.anchor = SpriteAnchor::BodyCenter;
            }
            else if (anchor != "feet")
            {
                throw std::invalid_argument("unknown sprite anchor '" + anchor + "'");
            }
        }
        return sprite;
    }
    std::string readContentFile(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file)
        {
            throw std::invalid_argument("Could not open content file '" + path.string() + "'");
        }
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }
}
