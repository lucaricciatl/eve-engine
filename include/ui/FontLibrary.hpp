#pragma once

#include <imgui.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace vkui {

struct FontLoadResult {
    std::filesystem::path path;
    float size{18.0f};
    bool success{false};
};

class FontLibrary {
public:
    // Load a font into ImGui. If fontDirectory is provided, prefer fonts inside it
    // (files directly under the folder or common subfolders such as Montserrat/static).
    // Falls back to bundled Montserrat/Roboto static fonts relative to the executable.
    // Returns details about the chosen font and whether loading succeeded.
    static FontLoadResult loadFont(ImGuiIO& io,
                                   const std::filesystem::path& fontDirectory = {},
                                   float size = 18.0f);
};

} // namespace vkui
