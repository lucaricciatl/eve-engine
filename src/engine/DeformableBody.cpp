#include "engine/DeformableBody.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>

#include <algorithm>

namespace vkengine {
namespace {
constexpr float MIN_LENGTH = 1e-4f;
constexpr float MAX_DT = 1.0f / 30.0f;
}

DeformableBody::DeformableBody(int width, int height, float vertexSpacing)
    : clothWidth(std::max(2, width))
    , clothHeight(std::max(2, height))
    , spacing(std::max(0.05f, vertexSpacing))
{
    nodeData.resize(static_cast<std::size_t>(clothWidth * clothHeight));

    for (int y = 0; y < clothHeight; ++y) {
        for (int x = 0; x < clothWidth; ++x) {
            const std::size_t idx = index(x, y);
            nodeData[idx].position = glm::vec3{
                (x - clothWidth * 0.5f) * spacing,
                0.0f,
                (y - clothHeight * 0.5f) * spacing
            };
            nodeData[idx].velocity = glm::vec3(0.0f);
            nodeData[idx].pinned = false;
        }
    }

    for (int y = 0; y < clothHeight; ++y) {
        for (int x = 0; x < clothWidth; ++x) {
            if (x + 1 < clothWidth) {
                addSpring(x, y, x + 1, y, 1.0f);
            }
            if (y + 1 < clothHeight) {
                addSpring(x, y, x, y + 1, 1.0f);
            }
            if (x + 1 < clothWidth && y + 1 < clothHeight) {
                addSpring(x, y, x + 1, y + 1, 0.75f);
                addSpring(x + 1, y, x, y + 1, 0.75f);
            }
        }
    }

    meshIndices.reserve(static_cast<std::size_t>((clothWidth - 1) * (clothHeight - 1) * 6));
    for (int y = 0; y < clothHeight - 1; ++y) {
        for (int x = 0; x < clothWidth - 1; ++x) {
            const uint32_t i0 = static_cast<uint32_t>(index(x, y));
            const uint32_t i1 = static_cast<uint32_t>(index(x + 1, y));
            const uint32_t i2 = static_cast<uint32_t>(index(x, y + 1));
            const uint32_t i3 = static_cast<uint32_t>(index(x + 1, y + 1));
            meshIndices.push_back(i0);
            meshIndices.push_back(i1);
            meshIndices.push_back(i2);
            meshIndices.push_back(i1);
            meshIndices.push_back(i3);
            meshIndices.push_back(i2);
        }
    }
}

void DeformableBody::pinNode(int x, int y, bool value) noexcept
{
    if (x < 0 || x >= clothWidth || y < 0 || y >= clothHeight) {
        return;
    }
    nodeData[index(x, y)].pinned = value;
}

void DeformableBody::pinTopEdge(bool value) noexcept
{
    for (int x = 0; x < clothWidth; ++x) {
        pinNode(x, clothHeight - 1, value);
    }
}

void DeformableBody::addCentralImpulse(const glm::vec3& impulse) noexcept
{
    const std::size_t center = index(clothWidth / 2, clothHeight / 2);
    if (center < nodeData.size() && !nodeData[center].pinned) {
        nodeData[center].velocity += impulse;
    }
}

void DeformableBody::addSpring(int ax, int ay, int bx, int by, float stiffnessScale)
{
    const std::size_t ia = index(ax, ay);
    const std::size_t ib = index(bx, by);
    const glm::vec3 delta = nodeData[ib].position - nodeData[ia].position;
    const float rest = glm::length(delta);
    springs.push_back(Spring{ia, ib, rest, stiffness * stiffnessScale});
}

void DeformableBody::simulate(float deltaSeconds, const glm::vec3& gravity) noexcept
{
    if (deltaSeconds <= 0.0f) {
        return;
    }

    const float dt = std::min(deltaSeconds, MAX_DT);
    std::vector<glm::vec3> accelerations(nodeData.size(), gravity);

    for (const Spring& spring : springs) {
        const Node& a = nodeData[spring.a];
        const Node& b = nodeData[spring.b];
        glm::vec3 delta = b.position - a.position;
        float currentLength = glm::length(delta);
        if (currentLength < MIN_LENGTH) {
            continue;
        }
        glm::vec3 dir = delta / currentLength;
        float stretch = currentLength - spring.restLength;
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
        node.velocity *= (1.0f - damping * 0.5f);
        node.position += node.velocity * dt;

        if (node.position.y < floorY) {
            node.position.y = floorY;
            node.velocity.y = 0.0f;
        }
    }
}

void DeformableBody::computeVertexNormals(std::vector<glm::vec3>& outNormals) const
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
