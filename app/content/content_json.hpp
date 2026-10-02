#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <nlohmann/json.hpp>
#include <glm/vec2.hpp>
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    // Reads JSON shapes; content validators check rules such as quantities and names.
    // Missing optional fields keep the supplied default. Present but invalid fields fail.
    // sourceName and path identify the bad field; loaders may add the filename at their
    // outer boundary instead. Path helpers and failJson live in content_diagnostics.hpp.
    void checkJsonFields(
        const nlohmann::json& value,
        const std::vector<std::string_view>& allowed,
        std::string_view sourceName = {},
        std::string_view path = {});

    const nlohmann::json& requiredJsonMember(
        const nlohmann::json& object,
        std::string_view key,
        std::string_view sourceName = {},
        std::string_view path = {});

    // Returns the member, or nullptr when missing. A non-object value is an error.
    const nlohmann::json* optionalJsonMember(
        const nlohmann::json& object,
        std::string_view key,
        std::string_view sourceName = {},
        std::string_view path = {});

    int jsonInteger(
        const nlohmann::json& value,
        std::string_view sourceName = {},
        std::string_view path = {});

    int readInteger(
        const nlohmann::json& object,
        std::string_view key,
        std::string_view sourceName = {},
        std::string_view path = {});

    void readOptionalInteger(
        const nlohmann::json& object,
        std::string_view key,
        int& result,
        std::string_view sourceName = {},
        std::string_view path = {});

    // Leaves result unchanged when the key is missing; a present field must be an integer.
    void readOptionalInteger(
        const nlohmann::json& object,
        std::string_view key,
        std::optional<int>& result,
        std::string_view sourceName = {},
        std::string_view path = {});

    float jsonNumber(
        const nlohmann::json& value,
        std::string_view sourceName = {},
        std::string_view path = {});

    float readNumber(
        const nlohmann::json& object,
        std::string_view key,
        std::string_view sourceName = {},
        std::string_view path = {});

    void readOptionalNumber(
        const nlohmann::json& object,
        std::string_view key,
        float& result,
        std::string_view sourceName = {},
        std::string_view path = {});

    bool jsonBoolean(
        const nlohmann::json& value,
        std::string_view sourceName = {},
        std::string_view path = {});

    bool readBoolean(
        const nlohmann::json& object,
        std::string_view key,
        std::string_view sourceName = {},
        std::string_view path = {});

    void readOptionalBoolean(
        const nlohmann::json& object,
        std::string_view key,
        bool& result,
        std::string_view sourceName = {},
        std::string_view path = {});

    std::string jsonText(
        const nlohmann::json& value,
        std::string_view sourceName = {},
        std::string_view path = {});

    std::string readText(
        const nlohmann::json& object,
        std::string_view key,
        std::string_view sourceName = {},
        std::string_view path = {});

    void readOptionalText(
        const nlohmann::json& object,
        std::string_view key,
        std::string& result,
        std::string_view sourceName = {},
        std::string_view path = {});

    // Rejects an empty string. The description names the thing, as in "item name".
    std::string jsonName(
        const nlohmann::json& value,
        std::string_view description,
        std::string_view sourceName = {},
        std::string_view path = {});

    std::string readName(
        const nlohmann::json& object,
        std::string_view key,
        std::string_view description,
        std::string_view sourceName = {},
        std::string_view path = {});

    // Checks a two-element array. The shape is quoted back, as in "[x, y]".
    void checkJsonPair(
        const nlohmann::json& value,
        std::string_view shape,
        std::string_view sourceName = {},
        std::string_view path = {});

    glm::vec2 jsonVector(
        const nlohmann::json& value,
        std::string_view sourceName = {},
        std::string_view path = {});

    glm::vec2 readVector(
        const nlohmann::json& object,
        std::string_view key,
        std::string_view sourceName = {},
        std::string_view path = {});

    void readOptionalVector(
        const nlohmann::json& object,
        std::string_view key,
        glm::vec2& result,
        std::string_view sourceName = {},
        std::string_view path = {});

    // Reads the {"position", "size"} source rectangle shared by sprites and animation
    // frames. Callers check the surrounding fields, which differ between them.
    SpriteRegion jsonSpriteRegion(
        const nlohmann::json& value,
        std::string_view sourceName = {},
        std::string_view path = {});

    // Sprites and actor definitions share the "feet"/"center" spelling.
    void readOptionalSpriteAnchor(
        const nlohmann::json& object,
        std::string_view key,
        SpriteAnchor& result,
        std::string_view sourceName = {},
        std::string_view path = {});

    Sprite jsonSprite(
        const nlohmann::json& value,
        std::string_view sourceName = {},
        std::string_view path = {});

    void readOptionalSprite(
        const nlohmann::json& object,
        std::string_view key,
        Sprite& result,
        std::string_view sourceName = {},
        std::string_view path = {});

    void readOptionalSprite(
        const nlohmann::json& object,
        std::string_view key,
        std::optional<Sprite>& result,
        std::string_view sourceName = {},
        std::string_view path = {});

    void checkJsonObject(
        const nlohmann::json& value,
        std::string_view sourceName = {},
        std::string_view path = {});

    std::string loadContentText(const std::filesystem::path& path);

    // Reports syntax errors with the line and column the reader has to go and fix.
    nlohmann::json parseContentRoot(std::string_view text, std::string_view sourceName);
}
