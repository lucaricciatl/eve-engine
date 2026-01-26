#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace vkcore::fs {

using Path = std::filesystem::path;

struct Entry {
    Path path;
    bool isDirectory{false};
};

// Returns true if the path exists.
bool exists(const Path& p);

// Reads the entire file into a string. Returns std::nullopt on error.
std::optional<std::string> readText(const Path& p, std::error_code* ecOut = nullptr);

// Writes text to the file, creating parent directories if requested. Returns false on failure.
bool writeText(const Path& p, const std::string& content, bool createParents = true, std::error_code* ecOut = nullptr);

// Lists entries in a directory (non-recursive). Returns std::nullopt on error.
std::optional<std::vector<Entry>> list(const Path& dir, std::error_code* ecOut = nullptr);

// Creates directories (like std::filesystem::create_directories). Returns true if created or already exists.
bool createDirectories(const Path& dir, std::error_code* ecOut = nullptr);

// Removes a file or directory recursively. Returns count removed, or std::nullopt on error.
std::optional<uintmax_t> removeAll(const Path& p, std::error_code* ecOut = nullptr);

// Joins multiple path segments.
template <class... Parts>
Path join(const Path& first, Parts&&... parts)
{
    Path p = first;
    (p /= ... /= std::forward<Parts>(parts));
    return p;
}

} // namespace vkcore::fs
