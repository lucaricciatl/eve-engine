#include "engine/MolecularDynamics.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace vkengine {

MdMaterial MdMaterial::copper()
{
    MdMaterial material{};
    material.mass = 63.546f;
    material.sigma = 1.0f;
    material.epsilon = 0.0103f;
    material.cutoffRadius = 3.0f; // ~3 * sigma
    material.damping = 0.125f;
    return material;
}

MdMaterial MdMaterial::argon()
{
    // Typical Lennard-Jones parameters for argon-like noble gas
    MdMaterial material{};
    material.mass = 39.948f;       // g/mol equivalent mass unit
    material.sigma = 3.4f;         // angstrom-scale sigma
    material.epsilon = 0.0104f;    // depth in same units as rest of sim
    material.cutoffRadius = 8.5f;  // ~2.5 * sigma
    material.damping = 0.02f;      // light damping for stability
    return material;
}

MolecularDynamicsSimulation::MolecularDynamicsSimulation()
{
    refreshDerivedMaterialValues();
}

void MolecularDynamicsSimulation::setMaterial(const MdMaterial& material) noexcept
{
    materialParams = material;
    refreshDerivedMaterialValues();
}

void MolecularDynamicsSimulation::setMaxSubsteps(std::uint32_t substeps) noexcept
{
    maxSubsteps = std::max<std::uint32_t>(1, substeps);
}

void MolecularDynamicsSimulation::setMaxTimeStep(float maxStepSeconds) noexcept
{
    const float minStep = 1e-5f;
    maxTimeStep = std::max(minStep, maxStepSeconds);
}

void MolecularDynamicsSimulation::setMinimumInteractionDistance(float distance) noexcept
{
    const float minValue = 1e-4f;
    minDistance = std::max(minValue, distance);
}

void MolecularDynamicsSimulation::setMaxSpeed(float speed) noexcept
{
    const float minSpeed = 0.1f;
    maxSpeed = std::max(minSpeed, speed);
}

void MolecularDynamicsSimulation::addKineticEnergy(float energyJoules) noexcept
{
    if (energyJoules <= 0.0f || atomBuffer.empty()) {
        return;
    }

    const double currentKe = computeKineticEnergy();
    const double targetKe = currentKe + static_cast<double>(energyJoules);
    if (targetKe <= 0.0) {
        return;
    }

    if (currentKe > 0.0) {
        const double scale = std::sqrt(targetKe / currentKe);
        for (auto& atom : atomBuffer) {
            atom.velocity *= static_cast<float>(scale);
            const float speed = glm::length(atom.velocity);
            if (speed > maxSpeed) {
                atom.velocity *= (maxSpeed / speed);
            }
        }
        return;
    }

    const double perAtomEnergy = targetKe / static_cast<double>(atomBuffer.size());
    for (std::size_t i = 0; i < atomBuffer.size(); ++i) {
        auto& atom = atomBuffer[i];
        const double invMass = 1.0 / std::max(static_cast<double>(atom.mass), 1e-8);
        const double speed = std::sqrt(std::max(0.0, 2.0 * perAtomEnergy * invMass));
        glm::vec3 dir{0.0f};
        const int axis = static_cast<int>(i % 3);
        const float sign = (i % 2 == 0) ? 1.0f : -1.0f;
        dir[axis] = sign;
        atom.velocity = dir * static_cast<float>(speed);
        const float vmag = glm::length(atom.velocity);
        if (vmag > maxSpeed) {
            atom.velocity *= (maxSpeed / vmag);
        }
    }
}

float MolecularDynamicsSimulation::kineticEnergy() const noexcept
{
    return static_cast<float>(computeKineticEnergy());
}

MdAtom& MolecularDynamicsSimulation::addAtom(const glm::vec3& position, const glm::vec3& velocity)
{
    MdAtom atom{};
    atom.position = position;
    atom.velocity = velocity;
    atom.mass = materialParams.mass;
    atomBuffer.emplace_back(atom);
    return atomBuffer.back();
}

void MolecularDynamicsSimulation::clearAtoms()
{
    atomBuffer.clear();
}

void MolecularDynamicsSimulation::step(float deltaSeconds)
{
    if (deltaSeconds <= 0.0f || atomBuffer.empty()) {
        return;
    }

    const float clampedDelta = std::max(deltaSeconds, 0.0f);
    std::uint32_t steps = 1;
    if (clampedDelta > maxTimeStep) {
        const float estimate = clampedDelta / maxTimeStep;
        steps = static_cast<std::uint32_t>(std::ceil(estimate));
        steps = std::min(steps, maxSubsteps);
    }

    const float dt = clampedDelta / static_cast<float>(steps);
    for (std::uint32_t i = 0; i < steps; ++i) {
        integrate(dt);
    }
}

void MolecularDynamicsSimulation::integrate(float dt)
{
    if (dt <= 0.0f || atomBuffer.empty()) {
        return;
    }

    computeForces();

    const float dampingFactor = std::max(0.0f, 1.0f - materialParams.damping * dt);

    for (auto& atom : atomBuffer) {
        const float invMass = 1.0f / std::max(atom.mass, 1e-4f);
        const glm::vec3 acceleration = atom.force * invMass;
        atom.velocity += 0.5f * acceleration * dt;
        atom.velocity *= dampingFactor;
        const float speed = glm::length(atom.velocity);
        if (speed > maxSpeed) {
            atom.velocity *= (maxSpeed / speed);
        }
        atom.position += atom.velocity * dt;
        applyBounds(atom);
    }

    computeForces();

    for (auto& atom : atomBuffer) {
        const float invMass = 1.0f / std::max(atom.mass, 1e-4f);
        const glm::vec3 acceleration = atom.force * invMass;
        atom.velocity += 0.5f * acceleration * dt;
        atom.velocity *= dampingFactor;
        const float speed = glm::length(atom.velocity);
        if (speed > maxSpeed) {
            atom.velocity *= (maxSpeed / speed);
        }
    }

}

void MolecularDynamicsSimulation::computeForces()
{
    if (atomBuffer.empty()) {
        return;
    }

    for (auto& atom : atomBuffer) {
        atom.force = glm::vec3(0.0f);
    }

    const float minDistanceSquared = minDistance * minDistance;

    for (std::size_t i = 0; i + 1 < atomBuffer.size(); ++i) {
        for (std::size_t j = i + 1; j < atomBuffer.size(); ++j) {
            glm::vec3 delta = atomBuffer[j].position - atomBuffer[i].position;
            float r2 = glm::dot(delta, delta);
            r2 = std::max(r2, minDistanceSquared);
            if (r2 > cutoffSquared) {
                continue;
            }

            const float invR2 = 1.0f / r2;
            const float invR6 = invR2 * invR2 * invR2;
            const float term = sigma6 * invR6;
            const float forceMag = 24.0f * materialParams.epsilon * invR2 * term * (2.0f * term - 1.0f);
            const glm::vec3 forceVec = forceMag * delta;

            atomBuffer[i].force += forceVec;
            atomBuffer[j].force -= forceVec;
        }
    }
}

void MolecularDynamicsSimulation::applyBounds(MdAtom& atom) const
{
    const glm::vec3 minB = simulationBounds.minBounds;
    const glm::vec3 maxB = simulationBounds.maxBounds;

    for (int axis = 0; axis < 3; ++axis) {
        if (atom.position[axis] < minB[axis]) {
            atom.position[axis] = minB[axis];
            atom.velocity[axis] = -atom.velocity[axis] * (1.0f - materialParams.damping);
        } else if (atom.position[axis] > maxB[axis]) {
            atom.position[axis] = maxB[axis];
            atom.velocity[axis] = -atom.velocity[axis] * (1.0f - materialParams.damping);
        }
    }
}

void MolecularDynamicsSimulation::refreshDerivedMaterialValues() noexcept
{
    const float safeSigma = std::max(0.05f, materialParams.sigma);
    sigma6 = std::pow(safeSigma, 6.0f);
    cutoffSquared = std::max(materialParams.cutoffRadius * materialParams.cutoffRadius, safeSigma * safeSigma);
    const float safetyMin = 0.1f * safeSigma;
    if (minDistance < safetyMin) {
        minDistance = safetyMin;
    }
}

double MolecularDynamicsSimulation::computeKineticEnergy() const noexcept
{
    if (atomBuffer.empty()) {
        return 0.0;
    }

    double kineticEnergy = 0.0;
    for (const auto& atom : atomBuffer) {
        const double speedSquared = static_cast<double>(glm::dot(atom.velocity, atom.velocity));
        kineticEnergy += 0.5 * static_cast<double>(atom.mass) * speedSquared;
    }

    return kineticEnergy;
}

} // namespace vkengine
