#include "core/primitives/PrimitiveGenerator.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <unordered_map>

namespace vkengine {
namespace {

constexpr float PI = glm::pi<float>();
constexpr float TWO_PI = 2.0f * PI;

void updateBounds(MeshData& mesh, const glm::vec3& position)
{
    mesh.boundsMin = glm::min(mesh.boundsMin, position);
    mesh.boundsMax = glm::max(mesh.boundsMax, position);
}

void initBounds(MeshData& mesh)
{
    mesh.boundsMin = glm::vec3(std::numeric_limits<float>::max());
    mesh.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
}

void finalizeBounds(MeshData& mesh)
{
    if (mesh.vertices.empty()) {
        mesh.boundsMin = glm::vec3(0.0f);
        mesh.boundsMax = glm::vec3(0.0f);
    }
}

void addVertex(MeshData& mesh, const glm::vec3& position, const glm::vec3& normal, const glm::vec2& uv)
{
    MeshVertex vertex{};
    vertex.position = position;
    vertex.normal = glm::normalize(normal);
    vertex.uv = uv;
    mesh.vertices.push_back(vertex);
    updateBounds(mesh, position);
}

void addTriangle(MeshData& mesh, std::uint32_t i0, std::uint32_t i1, std::uint32_t i2)
{
    mesh.indices.push_back(i0);
    mesh.indices.push_back(i1);
    mesh.indices.push_back(i2);
}

void addQuad(MeshData& mesh, std::uint32_t i0, std::uint32_t i1, std::uint32_t i2, std::uint32_t i3)
{
    addTriangle(mesh, i0, i1, i2);
    addTriangle(mesh, i0, i2, i3);
}

} // namespace

MeshData PrimitiveGenerator::createCube()
{
    MeshData mesh{};
    initBounds(mesh);

    // Half-size for unit cube
    constexpr float h = 0.5f;

    // Each face has its own vertices for correct normals
    // Front face (+Z)
    addVertex(mesh, {-h, -h,  h}, {0, 0, 1}, {0, 0});
    addVertex(mesh, { h, -h,  h}, {0, 0, 1}, {1, 0});
    addVertex(mesh, { h,  h,  h}, {0, 0, 1}, {1, 1});
    addVertex(mesh, {-h,  h,  h}, {0, 0, 1}, {0, 1});

    // Back face (-Z)
    addVertex(mesh, { h, -h, -h}, {0, 0, -1}, {0, 0});
    addVertex(mesh, {-h, -h, -h}, {0, 0, -1}, {1, 0});
    addVertex(mesh, {-h,  h, -h}, {0, 0, -1}, {1, 1});
    addVertex(mesh, { h,  h, -h}, {0, 0, -1}, {0, 1});

    // Top face (+Y)
    addVertex(mesh, {-h,  h,  h}, {0, 1, 0}, {0, 0});
    addVertex(mesh, { h,  h,  h}, {0, 1, 0}, {1, 0});
    addVertex(mesh, { h,  h, -h}, {0, 1, 0}, {1, 1});
    addVertex(mesh, {-h,  h, -h}, {0, 1, 0}, {0, 1});

    // Bottom face (-Y)
    addVertex(mesh, {-h, -h, -h}, {0, -1, 0}, {0, 0});
    addVertex(mesh, { h, -h, -h}, {0, -1, 0}, {1, 0});
    addVertex(mesh, { h, -h,  h}, {0, -1, 0}, {1, 1});
    addVertex(mesh, {-h, -h,  h}, {0, -1, 0}, {0, 1});

    // Right face (+X)
    addVertex(mesh, { h, -h,  h}, {1, 0, 0}, {0, 0});
    addVertex(mesh, { h, -h, -h}, {1, 0, 0}, {1, 0});
    addVertex(mesh, { h,  h, -h}, {1, 0, 0}, {1, 1});
    addVertex(mesh, { h,  h,  h}, {1, 0, 0}, {0, 1});

    // Left face (-X)
    addVertex(mesh, {-h, -h, -h}, {-1, 0, 0}, {0, 0});
    addVertex(mesh, {-h, -h,  h}, {-1, 0, 0}, {1, 0});
    addVertex(mesh, {-h,  h,  h}, {-1, 0, 0}, {1, 1});
    addVertex(mesh, {-h,  h, -h}, {-1, 0, 0}, {0, 1});

    // Create indices for all 6 faces (2 triangles each)
    for (std::uint32_t face = 0; face < 6; ++face) {
        std::uint32_t base = face * 4;
        addQuad(mesh, base, base + 1, base + 2, base + 3);
    }

    finalizeBounds(mesh);
    return mesh;
}

MeshData PrimitiveGenerator::createSphere(float radius, std::uint32_t sectors, std::uint32_t stacks)
{
    MeshData mesh{};
    initBounds(mesh);

    sectors = std::max(sectors, 3u);
    stacks = std::max(stacks, 2u);

    const float sectorStep = TWO_PI / static_cast<float>(sectors);
    const float stackStep = PI / static_cast<float>(stacks);

    // Generate vertices
    for (std::uint32_t i = 0; i <= stacks; ++i) {
        const float stackAngle = PI / 2.0f - static_cast<float>(i) * stackStep;
        const float xy = radius * std::cos(stackAngle);
        const float z = radius * std::sin(stackAngle);

        for (std::uint32_t j = 0; j <= sectors; ++j) {
            const float sectorAngle = static_cast<float>(j) * sectorStep;

            const float x = xy * std::cos(sectorAngle);
            const float y = xy * std::sin(sectorAngle);

            const glm::vec3 position{x, z, y};
            const glm::vec3 normal = glm::normalize(position);
            const glm::vec2 uv{
                static_cast<float>(j) / static_cast<float>(sectors),
                static_cast<float>(i) / static_cast<float>(stacks)
            };

            addVertex(mesh, position, normal, uv);
        }
    }

    // Generate indices
    for (std::uint32_t i = 0; i < stacks; ++i) {
        std::uint32_t k1 = i * (sectors + 1);
        std::uint32_t k2 = k1 + sectors + 1;

        for (std::uint32_t j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                addTriangle(mesh, k1, k2, k1 + 1);
            }
            if (i != stacks - 1) {
                addTriangle(mesh, k1 + 1, k2, k2 + 1);
            }
        }
    }

    finalizeBounds(mesh);
    return mesh;
}

MeshData PrimitiveGenerator::createCylinder(float radius, float height, std::uint32_t sectors, std::uint32_t capSegments)
{
    MeshData mesh{};
    initBounds(mesh);

    sectors = std::max(sectors, 3u);
    capSegments = std::max(capSegments, 1u);

    const float halfHeight = height * 0.5f;
    const float sectorStep = TWO_PI / static_cast<float>(sectors);

    // Side vertices
    std::uint32_t sideStartIndex = 0;
    for (std::uint32_t i = 0; i <= 1; ++i) {
        const float y = (i == 0) ? -halfHeight : halfHeight;
        const float v = static_cast<float>(i);

        for (std::uint32_t j = 0; j <= sectors; ++j) {
            const float sectorAngle = static_cast<float>(j) * sectorStep;
            const float x = radius * std::cos(sectorAngle);
            const float z = radius * std::sin(sectorAngle);

            const glm::vec3 position{x, y, z};
            const glm::vec3 normal = glm::normalize(glm::vec3{x, 0.0f, z});
            const glm::vec2 uv{static_cast<float>(j) / static_cast<float>(sectors), v};

            addVertex(mesh, position, normal, uv);
        }
    }

    // Side indices
    for (std::uint32_t j = 0; j < sectors; ++j) {
        std::uint32_t k1 = sideStartIndex + j;
        std::uint32_t k2 = k1 + sectors + 1;
        addQuad(mesh, k1, k2, k2 + 1, k1 + 1);
    }

    // Top cap
    std::uint32_t topCenterIndex = static_cast<std::uint32_t>(mesh.vertices.size());
    addVertex(mesh, {0.0f, halfHeight, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 0.5f});

    for (std::uint32_t j = 0; j <= sectors; ++j) {
        const float sectorAngle = static_cast<float>(j) * sectorStep;
        const float x = radius * std::cos(sectorAngle);
        const float z = radius * std::sin(sectorAngle);

        const glm::vec2 uv{0.5f + 0.5f * std::cos(sectorAngle), 0.5f + 0.5f * std::sin(sectorAngle)};
        addVertex(mesh, {x, halfHeight, z}, {0.0f, 1.0f, 0.0f}, uv);
    }

    for (std::uint32_t j = 0; j < sectors; ++j) {
        addTriangle(mesh, topCenterIndex, topCenterIndex + j + 1, topCenterIndex + j + 2);
    }

    // Bottom cap
    std::uint32_t bottomCenterIndex = static_cast<std::uint32_t>(mesh.vertices.size());
    addVertex(mesh, {0.0f, -halfHeight, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.5f});

    for (std::uint32_t j = 0; j <= sectors; ++j) {
        const float sectorAngle = static_cast<float>(j) * sectorStep;
        const float x = radius * std::cos(sectorAngle);
        const float z = radius * std::sin(sectorAngle);

        const glm::vec2 uv{0.5f + 0.5f * std::cos(sectorAngle), 0.5f - 0.5f * std::sin(sectorAngle)};
        addVertex(mesh, {x, -halfHeight, z}, {0.0f, -1.0f, 0.0f}, uv);
    }

    for (std::uint32_t j = 0; j < sectors; ++j) {
        addTriangle(mesh, bottomCenterIndex, bottomCenterIndex + j + 2, bottomCenterIndex + j + 1);
    }

    finalizeBounds(mesh);
    return mesh;
}

MeshData PrimitiveGenerator::createCone(float radius, float height, std::uint32_t sectors)
{
    MeshData mesh{};
    initBounds(mesh);

    sectors = std::max(sectors, 3u);

    const float halfHeight = height * 0.5f;
    const float sectorStep = TWO_PI / static_cast<float>(sectors);

    // Calculate slope for normals
    const float slopeAngle = std::atan2(radius, height);
    const float normalY = std::sin(slopeAngle);
    const float normalXZScale = std::cos(slopeAngle);

    // Apex vertex (repeated for each sector for proper normals)
    const glm::vec3 apex{0.0f, halfHeight, 0.0f};

    // Side triangles
    for (std::uint32_t j = 0; j < sectors; ++j) {
        const float angle1 = static_cast<float>(j) * sectorStep;
        const float angle2 = static_cast<float>(j + 1) * sectorStep;

        const float x1 = radius * std::cos(angle1);
        const float z1 = radius * std::sin(angle1);
        const float x2 = radius * std::cos(angle2);
        const float z2 = radius * std::sin(angle2);

        const glm::vec3 base1{x1, -halfHeight, z1};
        const glm::vec3 base2{x2, -halfHeight, z2};

        // Average normal for the face
        const float midAngle = (angle1 + angle2) * 0.5f;
        const glm::vec3 normal{
            normalXZScale * std::cos(midAngle),
            normalY,
            normalXZScale * std::sin(midAngle)
        };

        std::uint32_t baseIdx = static_cast<std::uint32_t>(mesh.vertices.size());

        addVertex(mesh, apex, normal, {0.5f, 1.0f});
        addVertex(mesh, base1, normal, {static_cast<float>(j) / static_cast<float>(sectors), 0.0f});
        addVertex(mesh, base2, normal, {static_cast<float>(j + 1) / static_cast<float>(sectors), 0.0f});

        addTriangle(mesh, baseIdx, baseIdx + 1, baseIdx + 2);
    }

    // Bottom cap
    std::uint32_t bottomCenterIndex = static_cast<std::uint32_t>(mesh.vertices.size());
    addVertex(mesh, {0.0f, -halfHeight, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.5f});

    for (std::uint32_t j = 0; j <= sectors; ++j) {
        const float sectorAngle = static_cast<float>(j) * sectorStep;
        const float x = radius * std::cos(sectorAngle);
        const float z = radius * std::sin(sectorAngle);

        const glm::vec2 uv{0.5f + 0.5f * std::cos(sectorAngle), 0.5f - 0.5f * std::sin(sectorAngle)};
        addVertex(mesh, {x, -halfHeight, z}, {0.0f, -1.0f, 0.0f}, uv);
    }

    for (std::uint32_t j = 0; j < sectors; ++j) {
        addTriangle(mesh, bottomCenterIndex, bottomCenterIndex + j + 2, bottomCenterIndex + j + 1);
    }

    finalizeBounds(mesh);
    return mesh;
}

MeshData PrimitiveGenerator::createTorus(float majorRadius, float minorRadius,
                                          std::uint32_t majorSegments, std::uint32_t minorSegments)
{
    MeshData mesh{};
    initBounds(mesh);

    majorSegments = std::max(majorSegments, 3u);
    minorSegments = std::max(minorSegments, 3u);

    const float majorStep = TWO_PI / static_cast<float>(majorSegments);
    const float minorStep = TWO_PI / static_cast<float>(minorSegments);

    // Generate vertices
    for (std::uint32_t i = 0; i <= majorSegments; ++i) {
        const float majorAngle = static_cast<float>(i) * majorStep;
        const float cosMajor = std::cos(majorAngle);
        const float sinMajor = std::sin(majorAngle);

        for (std::uint32_t j = 0; j <= minorSegments; ++j) {
            const float minorAngle = static_cast<float>(j) * minorStep;
            const float cosMinor = std::cos(minorAngle);
            const float sinMinor = std::sin(minorAngle);

            const float x = (majorRadius + minorRadius * cosMinor) * cosMajor;
            const float y = minorRadius * sinMinor;
            const float z = (majorRadius + minorRadius * cosMinor) * sinMajor;

            const glm::vec3 position{x, y, z};

            // Normal points from center of tube cross-section to vertex
            const glm::vec3 centerOnRing{majorRadius * cosMajor, 0.0f, majorRadius * sinMajor};
            const glm::vec3 normal = glm::normalize(position - centerOnRing);

            const glm::vec2 uv{
                static_cast<float>(i) / static_cast<float>(majorSegments),
                static_cast<float>(j) / static_cast<float>(minorSegments)
            };

            addVertex(mesh, position, normal, uv);
        }
    }

    // Generate indices
    for (std::uint32_t i = 0; i < majorSegments; ++i) {
        for (std::uint32_t j = 0; j < minorSegments; ++j) {
            std::uint32_t k1 = i * (minorSegments + 1) + j;
            std::uint32_t k2 = k1 + minorSegments + 1;

            addQuad(mesh, k1, k2, k2 + 1, k1 + 1);
        }
    }

    finalizeBounds(mesh);
    return mesh;
}

MeshData PrimitiveGenerator::createPlane(float width, float depth,
                                          std::uint32_t subdivisionsX, std::uint32_t subdivisionsZ)
{
    MeshData mesh{};
    initBounds(mesh);

    subdivisionsX = std::max(subdivisionsX, 1u);
    subdivisionsZ = std::max(subdivisionsZ, 1u);

    const float halfWidth = width * 0.5f;
    const float halfDepth = depth * 0.5f;

    const float stepX = width / static_cast<float>(subdivisionsX);
    const float stepZ = depth / static_cast<float>(subdivisionsZ);

    // Generate vertices
    for (std::uint32_t z = 0; z <= subdivisionsZ; ++z) {
        for (std::uint32_t x = 0; x <= subdivisionsX; ++x) {
            const float posX = -halfWidth + static_cast<float>(x) * stepX;
            const float posZ = -halfDepth + static_cast<float>(z) * stepZ;

            const glm::vec2 uv{
                static_cast<float>(x) / static_cast<float>(subdivisionsX),
                static_cast<float>(z) / static_cast<float>(subdivisionsZ)
            };

            addVertex(mesh, {posX, 0.0f, posZ}, {0.0f, 1.0f, 0.0f}, uv);
        }
    }

    // Generate indices
    for (std::uint32_t z = 0; z < subdivisionsZ; ++z) {
        for (std::uint32_t x = 0; x < subdivisionsX; ++x) {
            std::uint32_t topLeft = z * (subdivisionsX + 1) + x;
            std::uint32_t topRight = topLeft + 1;
            std::uint32_t bottomLeft = topLeft + subdivisionsX + 1;
            std::uint32_t bottomRight = bottomLeft + 1;

            addQuad(mesh, topLeft, bottomLeft, bottomRight, topRight);
        }
    }

    finalizeBounds(mesh);
    return mesh;
}

MeshData PrimitiveGenerator::createCapsule(float radius, float height, std::uint32_t sectors, std::uint32_t stacks)
{
    MeshData mesh{};
    initBounds(mesh);

    sectors = std::max(sectors, 3u);
    stacks = std::max(stacks, 2u);

    // Ensure height is at least 2 * radius
    const float cylinderHeight = std::max(0.0f, height - 2.0f * radius);
    const float halfCylinder = cylinderHeight * 0.5f;

    const float sectorStep = TWO_PI / static_cast<float>(sectors);
    const float stackStep = (PI * 0.5f) / static_cast<float>(stacks);

    // Top hemisphere
    for (std::uint32_t i = 0; i <= stacks; ++i) {
        const float stackAngle = PI * 0.5f - static_cast<float>(i) * stackStep;
        const float xy = radius * std::cos(stackAngle);
        const float y = radius * std::sin(stackAngle) + halfCylinder;

        for (std::uint32_t j = 0; j <= sectors; ++j) {
            const float sectorAngle = static_cast<float>(j) * sectorStep;
            const float x = xy * std::cos(sectorAngle);
            const float z = xy * std::sin(sectorAngle);

            const glm::vec3 position{x, y, z};
            const glm::vec3 normal = glm::normalize(glm::vec3{x, y - halfCylinder, z});
            const glm::vec2 uv{
                static_cast<float>(j) / static_cast<float>(sectors),
                0.5f - 0.5f * static_cast<float>(i) / static_cast<float>(stacks)
            };

            addVertex(mesh, position, normal, uv);
        }
    }

    // Cylinder section (just top and bottom rings, connected)
    std::uint32_t cylinderTopStart = static_cast<std::uint32_t>(mesh.vertices.size());
    for (std::uint32_t j = 0; j <= sectors; ++j) {
        const float sectorAngle = static_cast<float>(j) * sectorStep;
        const float x = radius * std::cos(sectorAngle);
        const float z = radius * std::sin(sectorAngle);

        const glm::vec3 normal = glm::normalize(glm::vec3{x, 0.0f, z});

        addVertex(mesh, {x, halfCylinder, z}, normal, {static_cast<float>(j) / static_cast<float>(sectors), 0.5f});
    }

    std::uint32_t cylinderBottomStart = static_cast<std::uint32_t>(mesh.vertices.size());
    for (std::uint32_t j = 0; j <= sectors; ++j) {
        const float sectorAngle = static_cast<float>(j) * sectorStep;
        const float x = radius * std::cos(sectorAngle);
        const float z = radius * std::sin(sectorAngle);

        const glm::vec3 normal = glm::normalize(glm::vec3{x, 0.0f, z});

        addVertex(mesh, {x, -halfCylinder, z}, normal, {static_cast<float>(j) / static_cast<float>(sectors), 0.5f});
    }

    // Bottom hemisphere
    std::uint32_t bottomHemiStart = static_cast<std::uint32_t>(mesh.vertices.size());
    for (std::uint32_t i = 0; i <= stacks; ++i) {
        const float stackAngle = -static_cast<float>(i) * stackStep;
        const float xy = radius * std::cos(stackAngle);
        const float y = radius * std::sin(stackAngle) - halfCylinder;

        for (std::uint32_t j = 0; j <= sectors; ++j) {
            const float sectorAngle = static_cast<float>(j) * sectorStep;
            const float x = xy * std::cos(sectorAngle);
            const float z = xy * std::sin(sectorAngle);

            const glm::vec3 position{x, y, z};
            const glm::vec3 normal = glm::normalize(glm::vec3{x, y + halfCylinder, z});
            const glm::vec2 uv{
                static_cast<float>(j) / static_cast<float>(sectors),
                0.5f + 0.5f * static_cast<float>(i) / static_cast<float>(stacks)
            };

            addVertex(mesh, position, normal, uv);
        }
    }

    // Top hemisphere indices
    for (std::uint32_t i = 0; i < stacks; ++i) {
        std::uint32_t k1 = i * (sectors + 1);
        std::uint32_t k2 = k1 + sectors + 1;

        for (std::uint32_t j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                addTriangle(mesh, k1, k2, k1 + 1);
            }
            addTriangle(mesh, k1 + 1, k2, k2 + 1);
        }
    }

    // Cylinder indices
    for (std::uint32_t j = 0; j < sectors; ++j) {
        addQuad(mesh, cylinderTopStart + j, cylinderBottomStart + j,
                cylinderBottomStart + j + 1, cylinderTopStart + j + 1);
    }

    // Bottom hemisphere indices
    for (std::uint32_t i = 0; i < stacks; ++i) {
        std::uint32_t k1 = bottomHemiStart + i * (sectors + 1);
        std::uint32_t k2 = k1 + sectors + 1;

        for (std::uint32_t j = 0; j < sectors; ++j, ++k1, ++k2) {
            addTriangle(mesh, k1, k2, k1 + 1);
            if (i != stacks - 1) {
                addTriangle(mesh, k1 + 1, k2, k2 + 1);
            }
        }
    }

    finalizeBounds(mesh);
    return mesh;
}

MeshData PrimitiveGenerator::createIcosphere(float radius, std::uint32_t subdivisions)
{
    MeshData mesh{};
    initBounds(mesh);

    subdivisions = std::min(subdivisions, 5u);  // Cap at 5 to avoid explosion

    // Golden ratio
    const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;

    // Initial icosahedron vertices (normalized)
    std::vector<glm::vec3> positions = {
        glm::normalize(glm::vec3{-1.0f,  t, 0.0f}),
        glm::normalize(glm::vec3{ 1.0f,  t, 0.0f}),
        glm::normalize(glm::vec3{-1.0f, -t, 0.0f}),
        glm::normalize(glm::vec3{ 1.0f, -t, 0.0f}),
        glm::normalize(glm::vec3{0.0f, -1.0f,  t}),
        glm::normalize(glm::vec3{0.0f,  1.0f,  t}),
        glm::normalize(glm::vec3{0.0f, -1.0f, -t}),
        glm::normalize(glm::vec3{0.0f,  1.0f, -t}),
        glm::normalize(glm::vec3{ t, 0.0f, -1.0f}),
        glm::normalize(glm::vec3{ t, 0.0f,  1.0f}),
        glm::normalize(glm::vec3{-t, 0.0f, -1.0f}),
        glm::normalize(glm::vec3{-t, 0.0f,  1.0f}),
    };

    // Store face indices as flat array (3 indices per face)
    std::vector<std::uint32_t> faceIndices = {
        0, 11, 5,   0, 5, 1,    0, 1, 7,    0, 7, 10,   0, 10, 11,
        1, 5, 9,    5, 11, 4,   11, 10, 2,  10, 7, 6,   7, 1, 8,
        3, 9, 4,    3, 4, 2,    3, 2, 6,    3, 6, 8,    3, 8, 9,
        4, 9, 5,    2, 4, 11,   6, 2, 10,   8, 6, 7,    9, 8, 1
    };

    // Helper to get midpoint index
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> midpointCache;

    auto getMidpoint = [&](std::uint32_t i1, std::uint32_t i2) -> std::uint32_t {
        auto key = std::minmax(i1, i2);
        auto it = midpointCache.find(key);
        if (it != midpointCache.end()) {
            return it->second;
        }

        glm::vec3 mid = glm::normalize((positions[i1] + positions[i2]) * 0.5f);
        std::uint32_t newIdx = static_cast<std::uint32_t>(positions.size());
        positions.push_back(mid);
        midpointCache[key] = newIdx;
        return newIdx;
    };

    // Subdivide
    for (std::uint32_t s = 0; s < subdivisions; ++s) {
        std::vector<std::uint32_t> newFaceIndices;
        newFaceIndices.reserve(faceIndices.size() * 4);

        for (size_t i = 0; i < faceIndices.size(); i += 3) {
            std::uint32_t v0 = faceIndices[i];
            std::uint32_t v1 = faceIndices[i + 1];
            std::uint32_t v2 = faceIndices[i + 2];

            std::uint32_t a = getMidpoint(v0, v1);
            std::uint32_t b = getMidpoint(v1, v2);
            std::uint32_t c = getMidpoint(v2, v0);

            // Face 0: v0, a, c
            newFaceIndices.push_back(v0);
            newFaceIndices.push_back(a);
            newFaceIndices.push_back(c);
            // Face 1: v1, b, a
            newFaceIndices.push_back(v1);
            newFaceIndices.push_back(b);
            newFaceIndices.push_back(a);
            // Face 2: v2, c, b
            newFaceIndices.push_back(v2);
            newFaceIndices.push_back(c);
            newFaceIndices.push_back(b);
            // Face 3: a, b, c
            newFaceIndices.push_back(a);
            newFaceIndices.push_back(b);
            newFaceIndices.push_back(c);
        }

        faceIndices = std::move(newFaceIndices);
        midpointCache.clear();
    }

    // Build mesh
    for (const auto& pos : positions) {
        glm::vec3 scaledPos = pos * radius;
        // Spherical UV mapping
        float u = 0.5f + std::atan2(pos.z, pos.x) / TWO_PI;
        float v = 0.5f - std::asin(pos.y) / PI;
        addVertex(mesh, scaledPos, pos, {u, v});
    }

    for (size_t i = 0; i < faceIndices.size(); i += 3) {
        addTriangle(mesh, faceIndices[i], faceIndices[i + 1], faceIndices[i + 2]);
    }

    finalizeBounds(mesh);
    return mesh;
}

} // namespace vkengine
