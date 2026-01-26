#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <vector>

namespace vkengine {

class DeformableBody {
public:
    struct Node {
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};
        bool pinned{false};
    };

    DeformableBody(int width, int height, float spacing);

    void simulate(float deltaSeconds, const glm::vec3& gravity) noexcept;

    void pinNode(int x, int y, bool value = true) noexcept;
    void pinTopEdge(bool value = true) noexcept;

    void addCentralImpulse(const glm::vec3& impulse) noexcept;

    void setStiffness(float value) noexcept { stiffness = value; }
    void setDamping(float value) noexcept { damping = value; }

    [[nodiscard]] int gridWidth() const noexcept { return clothWidth; }
    [[nodiscard]] int gridHeight() const noexcept { return clothHeight; }
    [[nodiscard]] float restSpacing() const noexcept { return spacing; }

    [[nodiscard]] const std::vector<Node>& nodes() const noexcept { return nodeData; }
    [[nodiscard]] const std::vector<uint32_t>& indices() const noexcept { return meshIndices; }

    void computeVertexNormals(std::vector<glm::vec3>& outNormals) const;

private:
    struct Spring {
        std::size_t a;
        std::size_t b;
        float restLength;
        float stiffness;
    };

    [[nodiscard]] std::size_t index(int x, int y) const noexcept { return static_cast<std::size_t>(y * clothWidth + x); }
    void addSpring(int ax, int ay, int bx, int by, float stiffnessScale = 1.0f);

private:
    int clothWidth;
    int clothHeight;
    float spacing;

    float massPerNode{0.3f};
    float stiffness{160.0f};
    float damping{0.08f};
    float floorY{-1.5f};

    std::vector<Node> nodeData;
    std::vector<Spring> springs;
    std::vector<uint32_t> meshIndices;
};

} // namespace vkengine
