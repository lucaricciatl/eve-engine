#include "engine/SoftBodyVolume.hpp"

#include <glm/gtx/norm.hpp>

#include <algorithm>
#include <cmath>

namespace vkengine {
namespace {
constexpr float MIN_LENGTH = 1e-5f;
constexpr float MAX_DT = 1.0f / 60.0f;
}

SoftBodyVolume::SoftBodyVolume(int sizeX, int sizeY, int sizeZ, float baseSpacing)
    : dimX(std::max(2, sizeX))
    , dimY(std::max(2, sizeY))
    , dimZ(std::max(2, sizeZ))
    , spacing(std::max(0.05f, baseSpacing))
{
    nodeData.resize(static_cast<std::size_t>(dimX * dimY * dimZ));

    const glm::vec3 halfExtent{
        (static_cast<float>(dimX) - 1.0f) * spacing * 0.5f,
        (static_cast<float>(dimY) - 1.0f) * spacing * 0.5f,
        (static_cast<float>(dimZ) - 1.0f) * spacing * 0.5f
    };

    for (int z = 0; z < dimZ; ++z) {
        for (int y = 0; y < dimY; ++y) {
            for (int x = 0; x < dimX; ++x) {
                const std::size_t idx = index(x, y, z);
                nodeData[idx].position = glm::vec3{
                    x * spacing - halfExtent.x,
                    y * spacing - halfExtent.y,
                    z * spacing - halfExtent.z
                };
                nodeData[idx].velocity = glm::vec3(0.0f);
                nodeData[idx].pinned = false;
            }
        }
    }

    const struct Offset {
        int dx;
        int dy;
        int dz;
        float stiffnessScale;
    } offsets[] = {
        {1, 0, 0, 1.0f}, {-1, 0, 0, 1.0f},
        {0, 1, 0, 1.0f}, {0, -1, 0, 1.0f},
        {0, 0, 1, 1.0f}, {0, 0, -1, 1.0f},
        {1, 1, 0, 0.8f}, {-1, -1, 0, 0.8f}, {1, -1, 0, 0.8f}, {-1, 1, 0, 0.8f},
        {1, 0, 1, 0.8f}, {-1, 0, -1, 0.8f}, {1, 0, -1, 0.8f}, {-1, 0, 1, 0.8f},
        {0, 1, 1, 0.8f}, {0, -1, -1, 0.8f}, {0, 1, -1, 0.8f}, {0, -1, 1, 0.8f},
        {1, 1, 1, 0.7f}, {-1, -1, -1, 0.7f},
        {1, 1, -1, 0.7f}, {1, -1, 1, 0.7f}, {-1, 1, 1, 0.7f},
        {-1, -1, 1, 0.7f}, {-1, 1, -1, 0.7f}, {1, -1, -1, 0.7f}
    };

    for (int z = 0; z < dimZ; ++z) {
        for (int y = 0; y < dimY; ++y) {
            for (int x = 0; x < dimX; ++x) {
                for (const auto& offset : offsets) {
                    const int nx = x + offset.dx;
                    const int ny = y + offset.dy;
                    const int nz = z + offset.dz;
                    if (nx < 0 || nx >= dimX || ny < 0 || ny >= dimY || nz < 0 || nz >= dimZ) {
                        continue;
                    }
                    addSpring(x, y, z, nx, ny, nz, offset.stiffnessScale);
                }
            }
        }
    }

    buildSurfaceMesh();
}

void SoftBodyVolume::pinLayerY(int layer, bool value) noexcept
{
    if (layer < 0 || layer >= dimY) {
        return;
    }
    for (int z = 0; z < dimZ; ++z) {
        for (int x = 0; x < dimX; ++x) {
            nodeData[index(x, layer, z)].pinned = value;
        }
    }
}

void SoftBodyVolume::addImpulse(const glm::vec3& impulse) noexcept
{
    if (nodeData.empty()) {
        return;
    }
    const glm::vec3 perNodeImpulse = impulse / static_cast<float>(nodeData.size());
    for (auto& node : nodeData) {
        if (!node.pinned) {
            node.velocity += perNodeImpulse;
        }
    }
}

void SoftBodyVolume::addSpring(int ax, int ay, int az, int bx, int by, int bz, float stiffnessScale)
{
    const std::size_t ia = index(ax, ay, az);
    const std::size_t ib = index(bx, by, bz);
    const glm::vec3 delta = nodeData[ib].position - nodeData[ia].position;
    const float rest = std::max(MIN_LENGTH, glm::length(delta));
    springs.push_back(Spring{ia, ib, rest, stiffness * stiffnessScale});
}

void SoftBodyVolume::simulate(float deltaSeconds, const glm::vec3& gravity) noexcept
{
    if (deltaSeconds <= 0.0f || nodeData.empty()) {
        return;
    }

    const float dt = std::min(deltaSeconds, MAX_DT);
    std::vector<glm::vec3> accelerations(nodeData.size(), gravity);

    for (const Spring& spring : springs) {
        const Node& a = nodeData[spring.a];
        const Node& b = nodeData[spring.b];
        glm::vec3 delta = b.position - a.position;
        float length = glm::length(delta);
        if (length < MIN_LENGTH) {
            continue;
        }
        glm::vec3 dir = delta / length;
        float stretch = length - spring.restLength;
        glm::vec3 force = spring.stiffness * stretch * dir;
        if (!a.pinned) {
            accelerations[spring.a] += force / massPerNode;
        }
        if (!b.pinned) {
            accelerations[spring.b] -= force / massPerNode;
        }
    }

    for (std::size_t i = 0; i < nodeData.size(); ++i) {
        Node& node = nodeData[i];
        if (node.pinned) {
            node.velocity = glm::vec3(0.0f);
            continue;
        }

        glm::vec3 accel = accelerations[i] - damping * node.velocity;
        node.velocity += accel * dt;
        node.position += node.velocity * dt;

        if (node.position.y < floorY) {
            node.position.y = floorY;
            if (node.velocity.y < 0.0f) {
                node.velocity.y *= -0.3f;
            }
        }
    }
}

void SoftBodyVolume::buildSurfaceMesh()
{
    meshIndices.clear();
    const auto emitQuad = [&](uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3) {
        meshIndices.push_back(i0);
        meshIndices.push_back(i1);
        meshIndices.push_back(i2);
        meshIndices.push_back(i0);
        meshIndices.push_back(i2);
        meshIndices.push_back(i3);
    };

    if (dimX < 2 || dimY < 2 || dimZ < 2) {
        return;
    }

    for (int y = 0; y < dimY - 1; ++y) {
        for (int x = 0; x < dimX - 1; ++x) {
            emitQuad(static_cast<uint32_t>(index(x, y, 0)),
                     static_cast<uint32_t>(index(x + 1, y, 0)),
                     static_cast<uint32_t>(index(x + 1, y + 1, 0)),
                     static_cast<uint32_t>(index(x, y + 1, 0)));
            emitQuad(static_cast<uint32_t>(index(x, y, dimZ - 1)),
                     static_cast<uint32_t>(index(x, y + 1, dimZ - 1)),
                     static_cast<uint32_t>(index(x + 1, y + 1, dimZ - 1)),
                     static_cast<uint32_t>(index(x + 1, y, dimZ - 1)));
        }
    }

    for (int z = 0; z < dimZ - 1; ++z) {
        for (int y = 0; y < dimY - 1; ++y) {
            emitQuad(static_cast<uint32_t>(index(0, y, z)),
                     static_cast<uint32_t>(index(0, y, z + 1)),
                     static_cast<uint32_t>(index(0, y + 1, z + 1)),
                     static_cast<uint32_t>(index(0, y + 1, z)));
            emitQuad(static_cast<uint32_t>(index(dimX - 1, y, z)),
                     static_cast<uint32_t>(index(dimX - 1, y + 1, z)),
                     static_cast<uint32_t>(index(dimX - 1, y + 1, z + 1)),
                     static_cast<uint32_t>(index(dimX - 1, y, z + 1)));
        }
    }

    for (int z = 0; z < dimZ - 1; ++z) {
        for (int x = 0; x < dimX - 1; ++x) {
            emitQuad(static_cast<uint32_t>(index(x, 0, z)),
                     static_cast<uint32_t>(index(x, 0, z + 1)),
                     static_cast<uint32_t>(index(x + 1, 0, z + 1)),
                     static_cast<uint32_t>(index(x + 1, 0, z)));
            emitQuad(static_cast<uint32_t>(index(x, dimY - 1, z)),
                     static_cast<uint32_t>(index(x + 1, dimY - 1, z)),
                     static_cast<uint32_t>(index(x + 1, dimY - 1, z + 1)),
                     static_cast<uint32_t>(index(x, dimY - 1, z + 1)));
        }
    }
}

void SoftBodyVolume::computeVertexNormals(std::vector<glm::vec3>& outNormals) const
{
    outNormals.assign(nodeData.size(), glm::vec3(0.0f));
    for (std::size_t i = 0; i + 2 < meshIndices.size(); i += 3) {
        const uint32_t ia = meshIndices[i];
        const uint32_t ib = meshIndices[i + 1];
        const uint32_t ic = meshIndices[i + 2];
        const glm::vec3& a = nodeData[ia].position;
        const glm::vec3& b = nodeData[ib].position;
        const glm::vec3& c = nodeData[ic].position;
        glm::vec3 normal = glm::cross(b - a, c - a);
        if (glm::length2(normal) < MIN_LENGTH) {
            continue;
        }
        outNormals[ia] += normal;
        outNormals[ib] += normal;
        outNormals[ic] += normal;
    }
    for (auto& normal : outNormals) {
        if (glm::length2(normal) > MIN_LENGTH) {
            normal = glm::normalize(normal);
        } else {
            normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }
}

} // namespace vkengine
