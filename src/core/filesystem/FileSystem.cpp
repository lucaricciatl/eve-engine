#include "core/filesystem/FileSystem.hpp"

#include <fstream>

namespace vkcore::fs {

bool exists(const Path& p)
{
    std::error_code ec;
    const bool ok = std::filesystem::exists(p, ec);
    return !ec && ok;
}

std::optional<std::string> readText(const Path& p, std::error_code* ecOut)
{
    std::error_code ec;
    auto size = std::filesystem::file_size(p, ec);
    if (ec) {
        if (ecOut) *ecOut = ec;
        return std::nullopt;
    }

    std::ifstream file(p, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    std::string content;
    content.resize(static_cast<size_t>(size));
    file.read(content.data(), static_cast<std::streamsize>(content.size()));
    if (!file) {
        return std::nullopt;
    }
    return content;
}

bool writeText(const Path& p, const std::string& content, bool createParents, std::error_code* ecOut)
{
    std::error_code ec;
    if (createParents) {
        std::filesystem::create_directories(p.parent_path(), ec);
        if (ec) {
            if (ecOut) *ecOut = ec;
            return false;
        }
    }

    std::ofstream file(p, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!file) {
        return false;
    }
    if (ecOut) *ecOut = {};
    return true;
}

std::optional<std::vector<Entry>> list(const Path& dir, std::error_code* ecOut)
{
    std::error_code ec;
    std::vector<Entry> entries;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        entries.push_back({entry.path(), entry.is_directory()});
    }
    if (ec) {
        if (ecOut) *ecOut = ec;
        return std::nullopt;
    }
    return entries;
}

bool createDirectories(const Path& dir, std::error_code* ecOut)
{
    std::error_code ec;
    const bool created = std::filesystem::create_directories(dir, ec);
    if (ec) {
        if (ecOut) *ecOut = ec;
        return false;
    }
    if (ecOut) *ecOut = {};
    return created || std::filesystem::exists(dir);
}

std::optional<uintmax_t> removeAll(const Path& p, std::error_code* ecOut)
{
    std::error_code ec;
    const auto count = std::filesystem::remove_all(p, ec);
    if (ec) {
        if (ecOut) *ecOut = ec;
        return std::nullopt;
    }
    if (ecOut) *ecOut = {};
    return count;
}

} // namespace vkcore::fs
