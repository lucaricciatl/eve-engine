#include "engine/assets/MeshLoader.hpp"

#include <glm/gtx/norm.hpp>

#include <array>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace vkengine {
namespace {

glm::vec2 computePlanarUV(const glm::vec3& normal, const glm::vec3& position)
{
    const glm::vec3 absNormal = glm::abs(normal);
    if (absNormal.x >= absNormal.y && absNormal.x >= absNormal.z) {
        return {position.y, position.z};
    }
    if (absNormal.y >= absNormal.x && absNormal.y >= absNormal.z) {
        return {position.x, position.z};
    }
    return {position.x, position.y};
}

void appendTriangle(MeshData& mesh, const glm::vec3& rawNormal,
                    const std::array<glm::vec3, 3>& verts)
{
    glm::vec3 normal = rawNormal;
    if (glm::length2(normal) < 1e-8f) {
        normal = glm::normalize(glm::cross(verts[1] - verts[0], verts[2] - verts[0]));
    } else {
        normal = glm::normalize(normal);
    }

    for (const auto& position : verts) {
        MeshVertex vertex{};
        vertex.position = position;
        vertex.normal = normal;
        vertex.uv = computePlanarUV(normal, position);
        mesh.vertices.push_back(vertex);
        mesh.indices.push_back(static_cast<std::uint32_t>(mesh.vertices.size() - 1));

        mesh.boundsMin = glm::min(mesh.boundsMin, position);
        mesh.boundsMax = glm::max(mesh.boundsMax, position);
    }
}

MeshData parseBinaryStl(std::ifstream& stream, std::uint32_t triangleCount)
{
    MeshData mesh{};
    mesh.boundsMin = glm::vec3(std::numeric_limits<float>::max());
    mesh.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
    mesh.vertices.reserve(static_cast<size_t>(triangleCount) * 3);
    mesh.indices.reserve(static_cast<size_t>(triangleCount) * 3);

    stream.seekg(84, std::ios::beg);
    for (std::uint32_t i = 0; i < triangleCount; ++i) {
        std::array<float, 3> normalFloats{};
        stream.read(reinterpret_cast<char*>(normalFloats.data()), sizeof(float) * 3);
        glm::vec3 normal{normalFloats[0], normalFloats[1], normalFloats[2]};

        std::array<glm::vec3, 3> verts{};
        for (auto& vert : verts) {
            std::array<float, 3> coords{};
            stream.read(reinterpret_cast<char*>(coords.data()), sizeof(float) * 3);
            vert = {coords[0], coords[1], coords[2]};
        }

        std::uint16_t attributeByteCount = 0;
        stream.read(reinterpret_cast<char*>(&attributeByteCount), sizeof(attributeByteCount));
        (void)attributeByteCount;

        appendTriangle(mesh, normal, verts);
    }

    if (mesh.vertices.empty()) {
        mesh.boundsMin = glm::vec3(0.0f);
        mesh.boundsMax = glm::vec3(0.0f);
    }
    return mesh;
}

MeshData parseAsciiStl(std::ifstream& stream)
{
    MeshData mesh{};
    mesh.boundsMin = glm::vec3(std::numeric_limits<float>::max());
    mesh.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());

    std::string line;
    glm::vec3 currentNormal{0.0f, 1.0f, 0.0f};
    std::array<glm::vec3, 3> verts{};
    std::size_t vertIndex = 0;

    while (std::getline(stream, line)) {
        if (line.empty()) {
            continue;
        }
        std::stringstream ss(line);
        std::string token;
        ss >> token;
        if (token == "facet") {
            std::string normalKeyword;
            ss >> normalKeyword;
            ss >> currentNormal.x >> currentNormal.y >> currentNormal.z;
            vertIndex = 0;
        } else if (token == "vertex") {
            if (vertIndex >= 3) {
                continue;
            }
            ss >> verts[vertIndex].x >> verts[vertIndex].y >> verts[vertIndex].z;
            ++vertIndex;
            if (vertIndex == 3) {
                appendTriangle(mesh, currentNormal, verts);
                vertIndex = 0;
            }
        }
    }

    if (mesh.vertices.empty()) {
        mesh.boundsMin = glm::vec3(0.0f);
        mesh.boundsMax = glm::vec3(0.0f);
    }
    return mesh;
}

bool isAsciiStl(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Failed to open STL file: " + path.string());
    }

    std::array<char, 80> header{};
    stream.read(header.data(), header.size());
    if (!stream) {
        throw std::runtime_error("Failed to read STL header: " + path.string());
    }

    std::uint32_t triangleCount = 0;
    stream.read(reinterpret_cast<char*>(&triangleCount), sizeof(triangleCount));
    if (!stream) {
        throw std::runtime_error("Failed to read STL triangle count: " + path.string());
    }

    stream.seekg(0, std::ios::end);
    const std::streamoff fileSize = stream.tellg();
    const std::streamoff expectedSize = 84 + static_cast<std::streamoff>(triangleCount) * 50;
    return fileSize != expectedSize;
}

} // namespace

MeshData loadStlMesh(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("STL file does not exist: " + path.string());
    }

    if (isAsciiStl(path)) {
        std::ifstream stream(path);
        if (!stream) {
            throw std::runtime_error("Failed to open STL file: " + path.string());
        }
        return parseAsciiStl(stream);
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Failed to open STL file: " + path.string());
    }

    std::array<char, 80> header{};
    stream.read(header.data(), header.size());
    std::uint32_t triangleCount = 0;
    stream.read(reinterpret_cast<char*>(&triangleCount), sizeof(triangleCount));
    if (!stream) {
        throw std::runtime_error("Invalid STL binary header: " + path.string());
    }

    return parseBinaryStl(stream, triangleCount);
}

} // namespace vkengine
