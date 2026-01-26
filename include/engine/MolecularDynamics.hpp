#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace vkengine {

struct MdAtom {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 force{0.0f};
    float mass{1.0f};
};

struct MdMaterial {
    float mass{63.546f};
    float sigma{1.0f};
    float epsilon{0.0103f};
    float cutoffRadius{3.0f};
    float damping{0.01f};

    static MdMaterial copper();
    static MdMaterial argon();
};

struct MdBox {
    glm::vec3 minBounds{-8.0f};
    glm::vec3 maxBounds{8.0f};
};

class MolecularDynamicsSimulation {
public:
    MolecularDynamicsSimulation();

    void setMaterial(const MdMaterial& material) noexcept;
    [[nodiscard]] const MdMaterial& material() const noexcept { return materialParams; }

    void setBounds(const MdBox& bounds) noexcept { simulationBounds = bounds; }
    [[nodiscard]] const MdBox& bounds() const noexcept { return simulationBounds; }

    void setMaxSubsteps(std::uint32_t substeps) noexcept;
    void setMaxTimeStep(float maxStepSeconds) noexcept;
    void setMinimumInteractionDistance(float distance) noexcept;
    void setMaxSpeed(float speed) noexcept;
    void addKineticEnergy(float energyJoules) noexcept;
    [[nodiscard]] float kineticEnergy() const noexcept;

    MdAtom& addAtom(const glm::vec3& position, const glm::vec3& velocity = glm::vec3{0.0f});
    void clearAtoms();

    [[nodiscard]] std::vector<MdAtom>& atoms() noexcept { return atomBuffer; }
    [[nodiscard]] const std::vector<MdAtom>& atoms() const noexcept { return atomBuffer; }

    void step(float deltaSeconds);

private:
    void integrate(float dt);
    void computeForces();
    void applyBounds(MdAtom& atom) const;
    void refreshDerivedMaterialValues() noexcept;
    [[nodiscard]] double computeKineticEnergy() const noexcept;

private:
    MdMaterial materialParams{};
    MdBox simulationBounds{};
    std::vector<MdAtom> atomBuffer;
    float maxTimeStep{0.0015f};
    float maxSpeed{100.0f};
    float minDistance{0.2f};
    float sigma6{1.0f};
    float cutoffSquared{9.0f};
    std::uint32_t maxSubsteps{32};
};

} // namespace vkengine
