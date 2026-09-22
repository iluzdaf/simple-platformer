#include "animation_catalog.hpp"
#include "content_diagnostics.hpp"
#include "content_json.hpp"
#include "content_validation.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <nlohmann/json.hpp>
#include "simple_platformer/render/animation.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace simple_platformer
{
    namespace
    {
        struct ClipEntry
        {
            std::string_view name;
            AnimationName type;
        };

        // Every clip a set must supply, under the name the catalogue file uses for it.
        constexpr std::array<ClipEntry, 6> Clips = {
            {{"idle", AnimationName::Idle},
             {"move", AnimationName::Move},
             {"jump", AnimationName::Jump},
             {"fall", AnimationName::Fall},
             {"attack", AnimationName::Attack},
             {"death", AnimationName::Death}}};

        std::vector<std::string_view> clipNames()
        {
            std::vector<std::string_view> names;
            names.reserve(Clips.size());
            for (const ClipEntry& entry : Clips)
            {
                names.push_back(entry.name);
            }
            return names;
        }
    }

    void validateAnimationSet(const AnimationSet& set)
    {
        std::set<AnimationName> names;
        for (const auto& clip : set.clips)
        {
            std::string name;
            for (const ClipEntry& entry : Clips)
            {
                if (clip.name == entry.type)
                {
                    name = entry.name;
                    break;
                }
            }
            if (name.empty())
            {
                throw std::invalid_argument("unknown animation clip");
            }
            if (!names.insert(clip.name).second)
            {
                failJson({}, name, "duplicate animation clip");
            }
            if (clip.frames.empty())
            {
                failJson({}, fieldPath(name, "frames"), "expected at least one frame");
            }
            if (!std::isfinite(clip.frameDuration) || clip.frameDuration <= 0)
            {
                failJson({}, fieldPath(name, "frameDuration"), "expected a positive finite number");
            }
            for (std::size_t index = 0; index < clip.frames.size(); ++index)
            {
                try
                {
                    const auto& frame = clip.frames[index];
                    Sprite sprite;
                    sprite.region = frame;
                    sprite.size = frame.size;
                    validateContentSprite(sprite);
                    // Playback changes the source rectangle, not the sprite's display size.
                    if (frame.size != set.clips.front().frames.front().size)
                    {
                        throw std::invalid_argument("all frames in a set must use the same size");
                    }
                }
                catch (const std::invalid_argument& error)
                {
                    failJson({}, indexPath(fieldPath(name, "frames"), index), error.what());
                }
            }
        }
        for (const ClipEntry& entry : Clips)
        {
            if (names.count(entry.type) == 0)
            {
                throw std::invalid_argument(std::string(entry.name) + ": required clip is missing");
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
                failJson({}, fieldPath("animations", entry.first), error.what());
            }
        }
    }

    AnimationCatalog parseAnimationCatalog(std::string_view text, std::string_view sourceName)
    {
        const auto root = parseContentRoot(text, sourceName);
        checkJsonFields(root, {"animations"}, sourceName, "root");
        const auto& definitions = requiredJsonMember(root, "animations", sourceName, "root");
        checkJsonObject(definitions, sourceName, "animations");
        AnimationCatalog catalog;
        for (const auto& entry : definitions.items())
        {
            const std::string setPath = fieldPath("animations", entry.key());
            checkJsonFields(entry.value(), clipNames(), sourceName, setPath);
            AnimationSet set;
            for (const ClipEntry& definition : Clips)
            {
                const std::string name(definition.name);
                const std::string clipPath = fieldPath(setPath, name);
                const auto& value = requiredJsonMember(entry.value(), name, sourceName, setPath);
                checkJsonFields(
                    value, {"frames", "frameDuration", "looping"}, sourceName, clipPath);
                AnimationClip clip;
                clip.name = definition.type;
                clip.frameDuration = readNumber(value, "frameDuration", sourceName, clipPath);
                clip.looping = readBoolean(value, "looping", sourceName, clipPath);
                const auto& frames = requiredJsonMember(value, "frames", sourceName, clipPath);
                if (!frames.is_array())
                {
                    failJson(sourceName, fieldPath(clipPath, "frames"), "expected an array");
                }
                for (std::size_t frame = 0; frame < frames.size(); ++frame)
                {
                    const std::string framePath = indexPath(fieldPath(clipPath, "frames"), frame);
                    checkJsonFields(frames[frame], {"position", "size"}, sourceName, framePath);
                    clip.frames.push_back(jsonSpriteRegion(frames[frame], sourceName, framePath));
                }
                set.clips.push_back(clip);
            }
            catalog.emplace(entry.key(), set);
        }
        validateInFile(sourceName, [&] { validateAnimationCatalog(catalog); });
        return catalog;
    }

    AnimationCatalog loadAnimationCatalog(const std::filesystem::path& path)
    {
        return parseAnimationCatalog(loadContentText(path), path.string());
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
