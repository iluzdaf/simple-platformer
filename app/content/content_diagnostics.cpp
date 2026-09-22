#include "content_diagnostics.hpp"
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace simple_platformer
{
    void validateInFile(std::string_view sourceName, const std::function<void()>& validate)
    {
        try
        {
            validate();
        }
        catch (const std::invalid_argument& error)
        {
            failJson(sourceName, {}, error.what());
        }
    }

    std::string fieldPath(std::string_view path, std::string_view key)
    {
        if (path.empty())
        {
            return std::string(key);
        }
        std::string result(path);
        result += ".";
        result += key;
        return result;
    }

    std::string indexPath(std::string_view path, std::size_t index)
    {
        std::string result(path);
        result += "[";
        result += std::to_string(index);
        result += "]";
        return result;
    }

    void failJson(std::string_view sourceName, std::string_view path, std::string_view message)
    {
        std::string prefix;
        if (!sourceName.empty())
        {
            prefix += std::string(sourceName) + ": ";
        }
        if (!path.empty())
        {
            prefix += std::string(path) + ": ";
        }
        throw std::invalid_argument(prefix + std::string(message));
    }
}
