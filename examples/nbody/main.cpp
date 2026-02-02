#include "engine/GameEngine.hpp"
#include "engine/HeadlessCapture.hpp"
#include "examples/nbody/NBodyGpuInterop.hpp"
#include "core/VulkanRenderer.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtx/norm.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

namespace {

class NBodyEngine : public vkengine::GameEngine {
public:
    void initialize(std::size_t particleCount)
    {
        totalParticles = particleCount;
        auto& emitterObject = createObject("NBodyEmitter", vkengine::MeshType::Cube);
        emitterObject.render().visible = false;
        emitterRef = &emitterObject.enableParticleEmitter("NBody");
        emitterRef->setRenderMode(vkengine::ParticleRenderMode::Billboard);
        emitterRef->setShape(vkengine::ParticleShape::SoftCircle);
        emitterRef->setEmissionRate(0.0f);
        emitterRef->setMaxParticles(particleCount);
        emitterRef->setStartColor(glm::vec4(0.95f, 0.85f, 0.65f, 1.0f));
        emitterRef->setEndColor(glm::vec4(0.25f, 0.45f, 1.0f, 0.9f));
        gpuParams.solverSubsteps = 3;
        gpuParams.maxVelocity = 220.0f;
        gpuParams.maxDistance = 420.0f;
        emitterRef->setGravityConstant(gpuParams.gravityConstant);
        emitterRef->setSoftening(gpuParams.softening);
        emitterRef->setDamping(gpuParams.damping);
        emitterRef->setSolverSubsteps(gpuParams.solverSubsteps);
        emitterRef->setLifetimeRange(1.0e6f, 1.0e6f);

        // Seed particles after warm-start in update().
    }

    void update(float deltaSeconds) override
    {
        vkengine::GameEngine::update(deltaSeconds);
        if (!seeded) {
            seedParticles();
            seeded = true;
        }
    }

private:
    static glm::vec3 safeDirection(const glm::vec3& value)
    {
        const float len2 = glm::length2(value);
        if (!std::isfinite(len2) || len2 < 1e-8f) {
            return glm::vec3(0.0f, 1.0f, 0.0f);
        }
        return value / std::sqrt(len2);
    }

    void seedParticles()
    {
        if (emitterRef == nullptr) {
            return;
        }
        auto& particles = emitterRef->particles();
        const std::size_t count = std::min(totalParticles, particles.size());
        if (count == 0) {
            return;
        }

        std::uniform_real_distribution<float> radialDist(0.0f, 1.0f);
        std::uniform_real_distribution<float> verticalDist(-0.2f, 0.2f);
        std::uniform_real_distribution<float> spinDist(0.4f, 1.1f);
        std::uniform_real_distribution<float> massDist(0.4f, 1.6f);
        std::uniform_real_distribution<float> jitterDist(-0.5f, 0.5f);

        const float maxLifetime = std::numeric_limits<float>::max();
        for (std::size_t i = 0; i < count; ++i) {
            glm::vec3 seed = glm::ballRand(1.0f);
            const float radius = glm::mix(5.0f, 75.0f, radialDist(rng));
            seed = safeDirection(seed) * radius;
            seed.y *= 0.45f;
            seed.y += verticalDist(rng) * radius * 0.15f;

            const float mass = massDist(rng);
            glm::vec3 tangential = safeDirection(glm::cross(seed, glm::vec3(0.0f, 1.0f, 0.0f)));
            const float orbitalSpeed = spinDist(rng) * std::sqrt(std::max(0.05f, 60.0f / (glm::length(seed) + 2.0f)));
            glm::vec3 velocity = tangential * orbitalSpeed;
            velocity.y += jitterDist(rng) * 0.15f;

            auto& particle = particles[i];
            particle.position = seed;
            particle.velocity = velocity;
            particle.mass = mass;
            particle.scale = glm::mix(0.03f, 0.12f, (mass - 0.4f) / 1.2f);
            particle.age = 0.0f;
            particle.lifetime = maxLifetime;
        }
    }

private:
    vkengine::ParticleEmitter* emitterRef{nullptr};
    vkengine::NBodyGpuParams gpuParams{};
    std::size_t totalParticles{0};
    bool seeded{false};
    std::mt19937 rng{1337u};
};

} // namespace

int main(int argc, char** argv)
try {
    constexpr std::size_t PARTICLE_COUNT = 2000;

    NBodyEngine engine;

    auto& scene = engine.scene();
    scene.clear();
    engine.initialize(PARTICLE_COUNT);

    auto& camera = scene.camera();
    camera.setPosition(glm::vec3{0.0f, 45.0f, 140.0f});
    camera.setPerspective(65.0f, 0.1f, 600.0f);
    camera.lookAt(glm::vec3{0.0f});
    camera.setMovementSpeed(30.0f);
    camera.setRotationSpeed(glm::radians(80.0f));

    vkengine::LightCreateInfo keyLightInfo{};
    keyLightInfo.name = "CoreLight";
    keyLightInfo.position = glm::vec3{0.0f, 80.0f, 0.0f};
    keyLightInfo.color = glm::vec3{0.8f, 0.9f, 1.0f};
    keyLightInfo.intensity = 5.0f;
    auto& keyLight = scene.createLight(keyLightInfo);

    VulkanRenderer renderer(engine);
    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "NBodyExample");
    if (capture.enabled) {
        WindowConfig window{};
        window.width = capture.width;
        window.height = capture.height;
        window.headless = true;
        window.resizable = false;
        renderer.setWindowConfig(window);
        renderer.setSceneControlsEnabled(false);
        renderer.setLightAnimationEnabled(false);
        std::filesystem::create_directories(capture.outputPath.parent_path());
        return renderer.renderSingleFrameToJpeg(capture.outputPath) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    renderer.run();
    return EXIT_SUCCESS;
} catch (const std::exception& e) {
    std::cerr << "N-body example failed: " << e.what() << '\n';
    return EXIT_FAILURE;
}
