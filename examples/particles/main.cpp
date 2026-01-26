#include "engine/GameEngine.hpp"
#include "engine/HeadlessCapture.hpp"
#include "VulkanRenderer.hpp"

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
        emitter.setOrigin({0.0f, -1.0f, 0.0f});
        emitter.setDirection({0.0f, 1.0f, 0.0f});
        emitter.setEmissionRate(90.0f);
        emitter.setLifetimeRange(0.8f, 1.6f);
        emitter.setSpeedRange(1.5f, 4.0f);
        emitter.setSpread(glm::radians(35.0f));
        emitter.setMaxParticles(maxParticles);
    }
};

int main(int argc, char** argv)
try {
    ParticleDemoEngine engine;

    auto& scene = engine.scene();
    scene.clear();

    auto& camera = scene.camera();
    camera.setPosition(glm::vec3{0.0f, 1.5f, 5.0f});
    camera.setYawPitch(glm::radians(-180.0f), glm::radians(-10.0f));
    camera.setMovementSpeed(5.0f);

    auto& ground = engine.createObject("Ground", vkengine::MeshType::Cube);
    ground.transform().scale = glm::vec3{6.0f, 0.1f, 6.0f};
    ground.transform().position = glm::vec3{0.0f, -1.5f, 0.0f};
    ground.enableCollider(glm::vec3{3.0f, 0.05f, 3.0f}, true);

    vkengine::LightCreateInfo keyLightInfo{};
    keyLightInfo.name = "KeyLight";
    keyLightInfo.position = glm::vec3{2.0f, 4.0f, 1.0f};
    keyLightInfo.color = glm::vec3{1.0f, 0.9f, 0.7f};
    keyLightInfo.intensity = 5.0f;
    auto& keyLight = scene.createLight(keyLightInfo);

    // Keep the emitter count within the renderer's default particle vertex buffer budget
    engine.configureParticles(40);

    VulkanCubeApp renderer(engine);
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
        return renderer.renderSingleFrameToJpeg(capture.outputPath) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    renderer.run();
    return EXIT_SUCCESS;
} catch (const std::exception& e) {
    std::cerr << "Particle example failed: " << e.what() << '\n';
    return EXIT_FAILURE;
}
