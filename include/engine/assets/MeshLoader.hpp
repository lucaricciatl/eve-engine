#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace vkengine {

struct MeshVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f};
};

struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
};

[[nodiscard]] MeshData loadStlMesh(const std::filesystem::path& path);

} // namespace vkengine
