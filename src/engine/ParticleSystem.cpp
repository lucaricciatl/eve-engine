#include "engine/ParticleSystem.hpp"

#include "core/ParallelFor.hpp"
#include "core/ecs/Components.hpp"
#include "core/ecs/Registry.hpp"

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtc/random.hpp>
#include <glm/gtx/norm.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace vkengine {

ParticleEmitter::ParticleEmitter()
    : ParticleEmitter("Emitter")
{
}

ParticleEmitter::ParticleEmitter(std::string name)
    : emitterName(std::move(name))
{
    const auto seed = static_cast<std::mt19937::result_type>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    rng.seed(seed);
    setMaxParticles(128);
}

void ParticleEmitter::setLifetimeRange(float minLifetimeSeconds, float maxLifetimeSeconds) noexcept
{
    minLifetime = std::max(0.01f, minLifetimeSeconds);
    maxLifetime = std::max(minLifetime, maxLifetimeSeconds);
}

void ParticleEmitter::setSpeedRange(float minSpeedValue, float maxSpeedValue) noexcept
{
    minSpeed = std::max(0.0f, minSpeedValue);
    maxSpeed = std::max(minSpeed, maxSpeedValue);
}

void ParticleEmitter::setMassRange(float minMassValue, float maxMassValue) noexcept
{
    constexpr float kMinMass = 1e-4f;
    minMass = std::max(kMinMass, minMassValue);
    maxMass = std::max(minMass, maxMassValue);
}

void ParticleEmitter::setMaxParticles(std::size_t maxParticles)
{
    particlePool.resize(std::max<std::size_t>(1, maxParticles));
    accelerationScratch.resize(particlePool.size(), glm::vec3(0.0f));
    for (auto& particle : particlePool) {
        particle.age = particle.lifetime = 1.0f;
    }
    needsWarmStart = true;
}

void ParticleEmitter::update(float deltaSeconds)
{
    if (needsWarmStart) {
        warmStartParticles();
        needsWarmStart = false;
    }

    if (particlePool.empty() || deltaSeconds <= 0.0f) {
        return;
    }

    accumulator += emissionRate * deltaSeconds;

    if (accelerationScratch.size() < particlePool.size()) {
        accelerationScratch.resize(particlePool.size(), glm::vec3(0.0f));
    }

    std::vector<std::size_t> aliveIndices;
    aliveIndices.reserve(particlePool.size());
    for (std::size_t i = 0; i < particlePool.size(); ++i) {
        if (particlePool[i].alive()) {
            aliveIndices.push_back(i);
        }
    }

    if (!aliveIndices.empty()) {
        const std::uint32_t steps = std::max<std::uint32_t>(1, solverSubsteps);
        const float stepDt = deltaSeconds / static_cast<float>(steps);
        const float minMassValue = std::max(1e-4f, minMass);

        auto resetAccelerations = [&]() {
            for (auto idx : aliveIndices) {
                accelerationScratch[idx] = glm::vec3(0.0f);
            }
        };

        auto accumulateAccelerations = [&]() {
            resetAccelerations();
            const float gravitationalConstant = gravityConstant;
            for (std::size_t a = 0; a < aliveIndices.size(); ++a) {
                const std::size_t idxA = aliveIndices[a];
                Particle& particleA = particlePool[idxA];
                for (std::size_t b = a + 1; b < aliveIndices.size(); ++b) {
                    const std::size_t idxB = aliveIndices[b];
                    Particle& particleB = particlePool[idxB];
                    glm::vec3 offset = particleB.position - particleA.position;
                    float distSqr = glm::length2(offset) + softening;
                    float invDist = 1.0f / std::sqrt(distSqr);
                    float invDist3 = invDist * invDist * invDist;
                    glm::vec3 direction = offset * invDist3;
                    float massProduct = gravitationalConstant * particleA.mass * particleB.mass;
                    glm::vec3 force = direction * massProduct;
                    accelerationScratch[idxA] += force / std::max(minMassValue, particleA.mass);
                    accelerationScratch[idxB] -= force / std::max(minMassValue, particleB.mass);
                }
            }
        };

        for (std::uint32_t step = 0; step < steps; ++step) {
            accumulateAccelerations();
            for (auto idx : aliveIndices) {
                Particle& particle = particlePool[idx];
                particle.velocity += accelerationScratch[idx] * stepDt;
                particle.velocity *= damping;
                particle.position += particle.velocity * stepDt;
            }
        }

        for (auto idx : aliveIndices) {
            particlePool[idx].age += deltaSeconds;
        }
    }

    const std::size_t particlesToSpawn = static_cast<std::size_t>(accumulator);
    accumulator -= static_cast<float>(particlesToSpawn);

    std::size_t spawned = 0;
    for (auto& particle : particlePool) {
        if (spawned >= particlesToSpawn) {
            break;
        }
        if (!particle.alive()) {
            spawnParticle(particle);
            ++spawned;
        }
    }
}

void ParticleEmitter::spawnParticle(Particle& particle)
{
    particle.position = originPoint;
    particle.velocity = randomDirection() * glm::mix(minSpeed, maxSpeed, uniform(rng));
    particle.age = 0.0f;
    particle.lifetime = glm::mix(minLifetime, maxLifetime, uniform(rng));
    particle.scale = glm::mix(0.1f, 0.3f, uniform(rng));
    particle.mass = glm::mix(minMass, maxMass, uniform(rng));
}

glm::vec3 ParticleEmitter::randomDirection()
{
    if (spread <= 0.0f) {
        return baseDirection;
    }

    const glm::vec3 randomUnit = glm::sphericalRand(1.0f);
    const float t = uniform(rng);
    const float angle = spread * std::sqrt(t);
    const glm::vec3 axisCandidate = glm::cross(baseDirection, randomUnit);
    const float axisLengthSquared = glm::length2(axisCandidate);
    if (axisLengthSquared < 1e-6f) {
        return baseDirection;
    }

    const glm::vec3 axis = axisCandidate / std::sqrt(axisLengthSquared);
    const float cosAngle = std::cos(angle);
    const float sinAngle = std::sin(angle);
    const glm::vec3 rotated = baseDirection * cosAngle + axis * sinAngle;
    if (glm::length2(rotated) < 1e-6f) {
        return baseDirection;
    }

    return glm::normalize(rotated);
}

void ParticleEmitter::warmStartParticles()
{
    for (auto& particle : particlePool) {
        spawnParticle(particle);
        particle.age = particle.lifetime * uniform(rng);
    }
}

ParticleEmitter& ParticleSystem::createEmitter(const std::string& name)
{
    if (ecsRegistry == nullptr) {
        throw std::runtime_error("ParticleSystem requires an attached ECS registry before creating emitters");
    }
    auto entity = ecsRegistry->createEntity();
    auto& component = ecsRegistry->emplace<ParticleEmitterComponent>(entity);
    component.enabled = true;
    component.emitter = std::make_unique<ParticleEmitter>(name.empty() ? std::string{"Emitter"} : name);
    return *component.emitter;
}

void ParticleSystem::update(float deltaSeconds)
{
    if (ecsRegistry == nullptr || deltaSeconds <= 0.0f) {
        return;
    }

    std::vector<ParticleEmitter*> emitters;
    ecsRegistry->view<ParticleEmitterComponent>([&](core::ecs::Entity, ParticleEmitterComponent& component) {
        if (!component.enabled || !component.emitter) {
            return;
        }
        emitters.push_back(component.emitter.get());
    });

    core::parallelFor(emitters.size(), 4, [&](std::size_t index) {
        emitters[index]->update(deltaSeconds);
    });
}

} // namespace vkengine
