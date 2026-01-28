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
#include <random>
#include <vector>

namespace {

class NBodyEngine : public vkengine::GameEngine, public vkengine::INBodyGpuProvider {
public:
    void initialize(std::size_t particleCount)
    {
        gpuStates.resize(particleCount);
        gpuParams.solverSubsteps = 8;
        gpuParams.maxVelocity = 220.0f;
        gpuParams.maxDistance = 420.0f;

        std::uniform_real_distribution<float> radialDist(0.0f, 1.0f);
        std::uniform_real_distribution<float> verticalDist(-0.2f, 0.2f);
        std::uniform_real_distribution<float> spinDist(0.4f, 1.1f);
        std::uniform_real_distribution<float> massDist(0.4f, 1.6f);
        std::uniform_real_distribution<float> jitterDist(-0.5f, 0.5f);

        for (std::size_t i = 0; i < particleCount; ++i) {
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

            vkengine::NBodyParticleState state{};
            state.positionAndMass = glm::vec4(seed, mass);
            state.velocityAndAge = glm::vec4(velocity, std::fmod(static_cast<float>(i) * 0.005f, gpuParams.colorCycleSeconds));
            gpuStates[i] = state;
        }
    }

    void update(float deltaSeconds) override
    {
        vkengine::GameEngine::update(deltaSeconds);
    }

    [[nodiscard]] const std::vector<vkengine::NBodyParticleState>& gpuInitialStates() const noexcept override
    {
        return gpuStates;
    }

    [[nodiscard]] const vkengine::NBodyGpuParams& gpuSimParams() const noexcept override { return gpuParams; }

private:
    static glm::vec3 safeDirection(const glm::vec3& value)
    {
        const float len2 = glm::length2(value);
        if (!std::isfinite(len2) || len2 < 1e-8f) {
            return glm::vec3(0.0f, 1.0f, 0.0f);
        }
        return value / std::sqrt(len2);
    }

private:
    std::vector<vkengine::NBodyParticleState> gpuStates;
    vkengine::NBodyGpuParams gpuParams{};
    std::mt19937 rng{std::random_device{}()};
};

} // namespace

int main(int argc, char** argv)
try {
    constexpr std::size_t PARTICLE_COUNT = 16384;

    NBodyEngine engine;
    engine.initialize(PARTICLE_COUNT);

    auto& scene = engine.scene();
    scene.clear();

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

    VulkanCubeApp renderer(engine);
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
