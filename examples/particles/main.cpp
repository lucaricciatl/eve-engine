#include "engine/GameEngine.hpp"
#include "engine/HeadlessCapture.hpp"
#include "core/VulkanRenderer.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>

class ParticleDemoEngine : public vkengine::GameEngine {
public:
    void configureParticles(std::size_t maxParticles)
    {
        auto& emitter = particles().createEmitter("Fountain");
        emitter.setOrigin({0.0f, -1.1f, 0.0f});
        emitter.setDirection({0.0f, 1.0f, 0.0f});
        emitter.setEmissionRate(320.0f);
        emitter.setLifetimeRange(0.25f, 0.7f);
        emitter.setSpeedRange(1.4f, 3.2f);
        emitter.setSpread(glm::radians(18.0f));
        emitter.setGravityConstant(0.0f);
        emitter.setDamping(0.98f);
        emitter.setSolverSubsteps(1);
        emitter.setShape(vkengine::ParticleShape::SoftCircle);
        emitter.setColorGradient({1.0f, 0.95f, 0.7f, 1.0f}, {0.18f, 0.04f, 0.02f, 0.0f});
        emitter.setRenderMode(vkengine::ParticleRenderMode::Billboard);
        emitter.setMaxParticles(maxParticles);

        auto& meshEmitter = particles().createEmitter("EmberMesh");
        meshEmitter.setOrigin({0.4f, -1.1f, 0.0f});
        meshEmitter.setDirection({0.0f, 1.0f, 0.0f});
        meshEmitter.setEmissionRate(60.0f);
        meshEmitter.setLifetimeRange(0.6f, 1.2f);
        meshEmitter.setSpeedRange(0.6f, 1.6f);
        meshEmitter.setSpread(glm::radians(12.0f));
        meshEmitter.setGravityConstant(0.0f);
        meshEmitter.setDamping(0.985f);
        meshEmitter.setSolverSubsteps(1);
        meshEmitter.setColorGradient({1.0f, 0.5f, 0.15f, 1.0f}, {0.2f, 0.05f, 0.02f, 0.0f});
        meshEmitter.setRenderMode(vkengine::ParticleRenderMode::Mesh);
        meshEmitter.setMeshType(vkengine::MeshType::Cube);
        meshEmitter.setMeshScale(0.08f);
        meshEmitter.setMaxParticles(std::max<std::size_t>(1, maxParticles / 2));
    }
};

int main(int argc, char** argv)
try {
    ParticleDemoEngine engine;

    auto& scene = engine.scene();
    scene.clear();

    auto& camera = scene.camera();
    camera.setPosition(glm::vec3{0.0f, 1.5f, 5.0f});
    camera.lookAt(glm::vec3{0.0f, -0.5f, 0.0f});
    camera.setMovementSpeed(5.0f);

    auto& ground = engine.createObject("Ground", vkengine::MeshType::Cube);
    ground.transform().scale = glm::vec3{6.0f, 0.1f, 6.0f};
    ground.transform().position = glm::vec3{0.0f, -1.5f, 0.0f};
    ground.enableCollider(glm::vec3{3.0f, 0.05f, 3.0f}, true);

    vkengine::LightCreateInfo keyLightInfo{};
    keyLightInfo.name = "KeyLight";
    keyLightInfo.position = glm::vec3{2.0f, 4.0f, 1.0f};
    keyLightInfo.color = glm::vec3{1.0f, 0.9f, 0.7f};
    keyLightInfo.intensity = 2.5f;
    auto& keyLight = scene.createLight(keyLightInfo);

    // Keep the emitter count within the renderer's default particle vertex buffer budget
    engine.configureParticles(40);

    VulkanRenderer renderer(engine);
    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "ParticleExample");
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
        return renderer.renderFrameToJpegAt(capture.outputPath, 30) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    renderer.run();
    return EXIT_SUCCESS;
} catch (const std::exception& e) {
    std::cerr << "Particle example failed: " << e.what() << '\n';
    return EXIT_FAILURE;
}
