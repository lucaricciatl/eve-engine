#include "engine/GameEngine.hpp"
#include "engine/HeadlessCapture.hpp"
#include "core/VulkanRenderer.hpp"

#include <glm/glm.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv)
try {
    vkengine::GameEngine engine;

    auto& scene = engine.scene();
    scene.clear();

    auto& camera = scene.camera();
    camera.setPosition(glm::vec3{0.0f, 2.2f, 8.0f});
    camera.lookAt(glm::vec3{0.0f, 1.0f, 0.0f});
    camera.setMovementSpeed(6.0f);

    auto& ground = engine.createObject("Ground", vkengine::MeshType::Cube);
    ground.transform().scale = glm::vec3{10.0f, 0.2f, 10.0f};
    ground.transform().position = glm::vec3{0.0f, -1.6f, 0.0f};
    ground.setBaseColor(glm::vec3{0.9f, 0.9f, 0.92f});
    ground.enableCollider(glm::vec3{5.0f, 0.1f, 5.0f}, true);

    auto& mirror = engine.createObject("Mirror", vkengine::MeshType::Cube);
    mirror.transform().scale = glm::vec3{7.0f, 3.8f, 0.08f};
    mirror.transform().position = glm::vec3{0.0f, 1.4f, 0.2f};
    mirror.transform().rotation = glm::vec3{0.0f, 0.0f, 0.785398f};
    mirror.render().albedoTexture = "__reflection__";
    mirror.render().roughness = 0.05f;
    mirror.render().specular = 1.0f;
    mirror.render().opacity = 1.0f;
    mirror.render().baseColor = glm::vec4(0.95f, 0.97f, 1.0f, 1.0f);

    auto& cube = engine.createObject("Cube", vkengine::MeshType::Cube);
    cube.transform().position = glm::vec3{-1.4f, 0.3f, 3.0f};
    cube.transform().scale = glm::vec3{0.9f};
    cube.setBaseColor(glm::vec3{0.85f, 0.52f, 0.42f});

    auto& reflected = engine.createObject("CubeReflection", vkengine::MeshType::Cube);
    reflected.transform().position = glm::vec3{1.8f, 0.3f, 0.8f};
    reflected.transform().scale = glm::vec3{0.9f};
    reflected.setBaseColor(glm::vec3{0.55f, 0.42f, 0.36f});
    reflected.render().roughness = 0.25f;
    reflected.render().specular = 0.9f;

    vkengine::LightCreateInfo keyLight{};
    keyLight.name = "KeyLight";
    keyLight.position = glm::vec3{2.5f, 4.5f, 3.5f};
    keyLight.color = glm::vec3{1.0f, 0.95f, 0.9f};
    keyLight.intensity = 1.5f;
    scene.createLight(keyLight);

    vkengine::LightCreateInfo fillLight{};
    fillLight.name = "FillLight";
    fillLight.position = glm::vec3{-3.0f, 2.5f, 1.5f};
    fillLight.color = glm::vec3{0.65f, 0.75f, 1.0f};
    fillLight.intensity = 0.8f;
    scene.createLight(fillLight);

    std::cout << "\n=== Reflections Example ===\n";
    std::cout << "Mirror plane with a cube and its reflected twin.\n";
    std::cout << "Use mouse/right-button to look around.\n\n";

    VulkanRenderer renderer(engine);
    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "ReflectionsExample");
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
    std::cerr << "Reflections example failed: " << e.what() << '\n';
    return EXIT_FAILURE;
}
