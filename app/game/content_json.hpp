#pragma once

#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>
#include <glm/vec2.hpp>
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    // JSON shape checks only; domain validation runs on the parsed C++ definitions.
    void checkJsonFields(
        const nlohmann::json& value,
        std::initializer_list<std::string_view> allowed);
    int jsonInteger(const nlohmann::json& value);
    glm::vec2 jsonVector(const nlohmann::json& value);
    Sprite jsonSprite(const nlohmann::json& value);
    std::string readContentFile(const std::filesystem::path& path);
}
