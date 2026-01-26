#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace vkengine {

bool writeJpeg(const std::filesystem::path& path,
               int width,
               int height,
               const std::vector<std::uint8_t>& rgbaPixels,
               int quality = 90);

} // namespace vkengine
