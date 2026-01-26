#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace vkengine {

class SoftBodyVolume {
public:
    struct Node {
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};
        bool pinned{false};
    };

    SoftBodyVolume(int sizeX, int sizeY, int sizeZ, float spacing);

    void simulate(float deltaSeconds, const glm::vec3& gravity) noexcept;

    void setStiffness(float value) noexcept { stiffness = value; }
    void setDamping(float value) noexcept { damping = value; }
    void setMass(float value) noexcept { massPerNode = value; }
    void setFloorY(float value) noexcept { floorY = value; }

    void pinLayerY(int layer, bool value = true) noexcept;
    void addImpulse(const glm::vec3& impulse) noexcept;

    [[nodiscard]] int sizeX() const noexcept { return dimX; }
    [[nodiscard]] int sizeY() const noexcept { return dimY; }
    [[nodiscard]] int sizeZ() const noexcept { return dimZ; }
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

    [[nodiscard]] std::size_t index(int x, int y, int z) const noexcept
    {
        return static_cast<std::size_t>((z * dimY + y) * dimX + x);
    }

    void addSpring(int ax, int ay, int az, int bx, int by, int bz, float stiffnessScale = 1.0f);
    void buildSurfaceMesh();

private:
    int dimX;
    int dimY;
    int dimZ;
    float spacing;

    float massPerNode{0.35f};
    float stiffness{240.0f};
    float damping{0.12f};
    float floorY{-1.5f};

    std::vector<Node> nodeData;
    std::vector<Spring> springs;
    std::vector<uint32_t> meshIndices;
};

} // namespace vkengine
