#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace simple_platformer
{
    // Diagnostic formatting shared by the JSON readers and the authoring validators.
    // Deliberately free of any JSON dependency so validators stay usable for C++ content.
    // Builds the "parent.child" and "parent[index]" paths that appear in diagnostics.
    std::string fieldPath(std::string_view path, std::string_view key);
    std::string indexPath(std::string_view path, std::size_t index);
    // Reports "source: path: message", omitting either part when it is empty.
    [[noreturn]] void failJson(
        std::string_view sourceName,
        std::string_view path,
        std::string_view message);

    // Runs a validator that C++ built content shares, so it names the thing but not the
    // file, and puts the file name in front of whatever it rejects.
    void validateInFile(std::string_view sourceName, const std::function<void()>& validate);
}
