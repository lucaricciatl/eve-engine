#pragma once

#include "core/ecs/Components.hpp"
#include "core/ecs/Registry.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace core::ecs {
class Registry;
}

namespace vkengine {

struct Particle {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float age{0.0f};
    float lifetime{0.0f};
    float scale{0.1f};
    float mass{1.0f};

    [[nodiscard]] bool alive() const noexcept { return age < lifetime; }
};

class ParticleEmitter {
public:
    ParticleEmitter();
    explicit ParticleEmitter(std::string name);

    void setOrigin(const glm::vec3& origin) noexcept { originPoint = origin; }
    void setDirection(const glm::vec3& direction) noexcept
    {
        const float lengthSquared = glm::length2(direction);
        if (lengthSquared < 1e-6f) {
            baseDirection = glm::vec3{0.0f, 1.0f, 0.0f};
        } else {
            baseDirection = glm::normalize(direction);
        }
    }
    void setEmissionRate(float particlesPerSecond) noexcept { emissionRate = particlesPerSecond; }
    void setLifetimeRange(float minLifetimeSeconds, float maxLifetimeSeconds) noexcept;
    void setSpeedRange(float minSpeed, float maxSpeed) noexcept;
    void setSpread(float radians) noexcept { spread = radians; }
    void setMassRange(float minMassValue, float maxMassValue) noexcept;
    void setGravityConstant(float gravity) noexcept { gravityConstant = gravity; }
    void setSoftening(float value) noexcept { softening = std::max(0.0f, value); }
    void setDamping(float value) noexcept { damping = std::clamp(value, 0.0f, 1.0f); }
    void setSolverSubsteps(std::uint32_t substeps) noexcept { solverSubsteps = std::max<std::uint32_t>(1, substeps); }
    void setMaxParticles(std::size_t maxParticles);

    [[nodiscard]] const std::string& name() const noexcept { return emitterName; }
    [[nodiscard]] std::vector<Particle>& particles() noexcept { return particlePool; }
    [[nodiscard]] const std::vector<Particle>& particles() const noexcept { return particlePool; }
    [[nodiscard]] std::size_t maxParticles() const noexcept { return particlePool.size(); }

    void update(float deltaSeconds);

private:
    void spawnParticle(Particle& particle);
    glm::vec3 randomDirection();
    void warmStartParticles();

private:
    std::string emitterName;
    glm::vec3 originPoint{0.0f};
    glm::vec3 baseDirection{0.0f, 1.0f, 0.0f};
    float emissionRate{30.0f};
    float minLifetime{1.0f};
    float maxLifetime{3.0f};
    float minSpeed{1.0f};
    float maxSpeed{3.0f};
    float spread{glm::radians(25.0f)};
    float minMass{0.5f};
    float maxMass{2.0f};
    float gravityConstant{40.0f};
    float softening{0.25f};
    float damping{0.999f};
    std::uint32_t solverSubsteps{4};
    float accumulator{0.0f};
    std::vector<Particle> particlePool;
    std::vector<glm::vec3> accelerationScratch;
    bool needsWarmStart{true};

    std::mt19937 rng;
    std::uniform_real_distribution<float> uniform{0.0f, 1.0f};
};

class ParticleSystem {
public:
    void attachRegistry(core::ecs::Registry* registry) noexcept { ecsRegistry = registry; }

    ParticleEmitter& createEmitter(const std::string& name);

    void update(float deltaSeconds);

    template <typename Func>
    void forEachAliveParticle(Func&& func) const
    {
        if (ecsRegistry == nullptr) {
            return;
        }
        ecsRegistry->view<ParticleEmitterComponent>([&](auto, ParticleEmitterComponent& component) {
            if (!component.enabled || !component.emitter) {
                return;
            }
            for (const auto& particle : component.emitter->particles()) {
                if (particle.alive()) {
                    func(particle);
                }
            }
        });
    }

private:
    core::ecs::Registry* ecsRegistry{nullptr};
};

} // namespace vkengine
