#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace vkengine {

struct TextureData {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t channels{0};
    std::vector<std::uint8_t> pixels;
};

[[nodiscard]] TextureData loadTexture(const std::filesystem::path& path, bool flipVertical = true);

} // namespace vkengine
