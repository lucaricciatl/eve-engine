#include "ui/FontLibrary.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace vkui {
namespace {

bool isFontFile(const std::filesystem::directory_entry& entry)
{
    if (!entry.is_regular_file()) {
        return false;
    }
    const auto ext = entry.path().extension().string();
    return ext == ".ttf" || ext == ".otf";
}

void appendIfExists(std::vector<std::filesystem::path>& out, const std::filesystem::path& p)
{
    if (std::filesystem::exists(p)) {
        out.push_back(p);
    }
}

} // namespace

FontLoadResult FontLibrary::loadFont(ImGuiIO& io, const std::filesystem::path& fontDirectory, float size)
{
    io.Fonts->Clear();

    std::vector<std::filesystem::path> candidates;

    // User-specified directory or file
    if (!fontDirectory.empty()) {
        if (std::filesystem::is_regular_file(fontDirectory)) {
            appendIfExists(candidates, fontDirectory);
        } else if (std::filesystem::is_directory(fontDirectory)) {
            appendIfExists(candidates, fontDirectory / "Montserrat-Regular.ttf");
            appendIfExists(candidates, fontDirectory / "Montserrat" / "static" / "Montserrat-Regular.ttf");
            appendIfExists(candidates, fontDirectory / "Roboto-Regular.ttf");
            appendIfExists(candidates, fontDirectory / "Roboto" / "static" / "Roboto-Regular.ttf");

            for (const auto& entry : std::filesystem::directory_iterator(fontDirectory)) {
                if (isFontFile(entry)) {
                    appendIfExists(candidates, entry.path());
                }
            }
        }
    }

    // Bundled fallbacks (relative to binary/workdir)
    const std::array<std::filesystem::path, 9> fallbackList = {
        std::filesystem::path{"assets/fonts/Montserrat/static/Montserrat-Regular.ttf"},
        std::filesystem::path{"../assets/fonts/Montserrat/static/Montserrat-Regular.ttf"},
        std::filesystem::path{"../../assets/fonts/Montserrat/static/Montserrat-Regular.ttf"},
        std::filesystem::path{"assets/fonts/Roboto/static/Roboto_Condensed-Bold.ttf"},
        std::filesystem::path{"../assets/fonts/Roboto/static/Roboto_Condensed-Bold.ttf"},
        std::filesystem::path{"../../assets/fonts/Roboto/static/Roboto_Condensed-Bold.ttf"},
        std::filesystem::path{"assets/fonts/Roboto/static/Roboto-Regular.ttf"},
        std::filesystem::path{"../assets/fonts/Roboto/static/Roboto-Regular.ttf"},
        std::filesystem::path{"../../assets/fonts/Roboto/static/Roboto-Regular.ttf"}
    };

    for (const auto& p : fallbackList) {
        appendIfExists(candidates, p);
    }

    FontLoadResult result{};
    result.size = size;

    for (const auto& path : candidates) {
        if (!std::filesystem::exists(path)) {
            continue;
        }
        result.path = path;
        result.success = true;
        result.size = size;
        io.FontDefault = io.Fonts->AddFontFromFileTTF(path.string().c_str(), size);
        if (result.success) {
            break;
        }
    }

    if (!result.success) {
        io.FontDefault = io.Fonts->AddFontDefault();
        std::fprintf(stderr, "ImGui: no custom fonts found; using default font. Looked in %s\n", fontDirectory.empty() ? "(none)" : fontDirectory.string().c_str());
    } else {
        std::fprintf(stderr, "ImGui: loaded font %s (size %.1f)\n", result.path.string().c_str(), result.size);
    }

    return result;
}

} // namespace vkui
