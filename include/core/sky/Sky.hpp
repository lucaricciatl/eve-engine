#pragma once

#include <glm/glm.hpp>
#include <filesystem>
#include <optional>

#include "engine/assets/TextureLoader.hpp"

namespace vkcore::sky {

struct Settings {
    glm::vec4 color{0.02f, 0.02f, 0.04f, 1.0f};
};

// Loads a sky image and returns an average color (alpha=1) suitable for a clear color.
// Returns std::nullopt on failure.
std::optional<glm::vec4> loadSkyColorFromFile(const std::filesystem::path& path);

} // namespace vkcore::sky
