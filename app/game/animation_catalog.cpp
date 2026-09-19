#include "animation_catalog.hpp"
#include "content_json.hpp"
#include "content_validation.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    namespace
    {
        constexpr std::array<std::string_view, 6> ClipNames =
            {"idle", "move", "jump", "fall", "attack", "death"};
        constexpr std::array<AnimationName, 6> ClipTypes = {
            AnimationName::Idle,
            AnimationName::Move,
            AnimationName::Jump,
            AnimationName::Fall,
            AnimationName::Attack,
            AnimationName::Death};
    }

    void validateAnimationSet(const AnimationSet& set)
    {
        std::set<AnimationName> names;
        for (const auto& clip : set.clips)
        {
            if (!names.insert(clip.name).second)
            {
                throw std::invalid_argument("duplicate animation clip");
            }
            if (clip.frames.empty() || !std::isfinite(clip.frameDuration) ||
                clip.frameDuration <= 0)
            {
                throw std::invalid_argument(
                    "clips require frames and a positive finite frameDuration");
            }
            const auto size = clip.frames.front().size;
            for (const auto& frame : clip.frames)
            {
                Sprite sprite;
                sprite.region = frame;
                sprite.size = frame.size;
                validateContentSprite(sprite);
                // Playback changes the source rectangle, not the sprite's display size.
                if (frame.size != size)
                {
                    throw std::invalid_argument("frames within a clip must have the same size");
                }
            }
        }
        if (names.size() != ClipTypes.size())
        {
            throw std::invalid_argument(
                "each set requires idle, move, jump, fall, attack and death");
        }
        for (const auto name : ClipTypes)
        {
            const auto& clip = clipFor(set, name);
            if (clip.frames.front().size != set.clips.front().frames.front().size)
            {
                throw std::invalid_argument("all clips in a set must use the same frame size");
            }
        }
    }

    void validateAnimationCatalog(const AnimationCatalog& catalog)
    {
        for (const auto& entry : catalog)
        {
            try
            {
                if (entry.first.empty())
                {
                    throw std::invalid_argument("animation set name cannot be empty");
                }
                validateAnimationSet(entry.second);
            }
            catch (const std::invalid_argument& error)
            {
                throw std::invalid_argument("animations." + entry.first + ": " + error.what());
            }
        }
    }

    AnimationCatalog parseAnimationCatalog(std::string_view text, std::string_view sourceName)
    {
        try
        {
            const auto root = nlohmann::json::parse(text);
            checkJsonFields(root, {"animations"});
            const auto& definitions = root.at("animations");
            if (!definitions.is_object())
            {
                throw std::invalid_argument("animations: expected an object");
            }
            AnimationCatalog catalog;
            for (const auto& entry : definitions.items())
            {
                const std::string prefix = "animations." + entry.key();
                try
                {
                    checkJsonFields(
                        entry.value(), {"idle", "move", "jump", "fall", "attack", "death"});
                    AnimationSet set;
                    for (std::size_t index = 0; index < ClipNames.size(); ++index)
                    {
                        const std::string name(ClipNames[index]);
                        try
                        {
                            const auto& value = entry.value().at(name);
                            checkJsonFields(value, {"frames", "frameDuration", "looping"});
                            AnimationClip clip;
                            clip.name = ClipTypes[index];
                            if (!value.at("frameDuration").is_number() ||
                                !value.at("looping").is_boolean())
                            {
                                throw std::invalid_argument(
                                    "expected numeric frameDuration and boolean looping");
                            }
                            clip.frameDuration = value.at("frameDuration").get<float>();
                            clip.looping = value.at("looping").get<bool>();
                            const auto& frames = value.at("frames");
                            if (!frames.is_array())
                            {
                                throw std::invalid_argument("frames: expected an array");
                            }
                            for (std::size_t frame = 0; frame < frames.size(); ++frame)
                            {
                                try
                                {
                                    checkJsonFields(frames[frame], {"position", "size"});
                                    clip.frames.push_back(
                                        {jsonVector(frames[frame].at("position")),
                                         jsonVector(frames[frame].at("size"))});
                                }
                                catch (const std::exception& error)
                                {
                                    throw std::invalid_argument(
                                        "frames[" + std::to_string(frame) + "]: " + error.what());
                                }
                            }
                            set.clips.push_back(clip);
                        }
                        catch (const std::exception& error)
                        {
                            throw std::invalid_argument(name + ": " + error.what());
                        }
                    }
                    catalog.emplace(entry.key(), set);
                }
                catch (const std::exception& error)
                {
                    throw std::invalid_argument(prefix + ": " + error.what());
                }
            }
            validateAnimationCatalog(catalog);
            return catalog;
        }
        catch (const std::exception& error)
        {
            throw std::invalid_argument(std::string(sourceName) + ": " + error.what());
        }
    }

    AnimationCatalog loadAnimationCatalog(const std::filesystem::path& path)
    {
        return parseAnimationCatalog(readContentFile(path), path.string());
    }

    const AnimationSet& animationSet(const AnimationCatalog& catalog, const std::string& name)
    {
        const auto found = catalog.find(name);
        if (found == catalog.end())
        {
            throw std::invalid_argument("unknown animation set '" + name + "'");
        }
        return found->second;
    }
}
