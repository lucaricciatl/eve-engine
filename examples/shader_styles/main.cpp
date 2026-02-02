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
    camera.setPosition(glm::vec3{0.0f, 3.5f, 12.0f});
    camera.lookAt(glm::vec3{0.0f, 1.0f, 0.0f});
    camera.setMovementSpeed(6.0f);

    auto& ground = engine.createObject("Ground", vkengine::MeshType::Cube);
    ground.transform().scale = glm::vec3{16.0f, 0.2f, 16.0f};
    ground.transform().position = glm::vec3{0.0f, -1.5f, 0.0f};
    ground.enableCollider(glm::vec3{8.0f, 0.1f, 8.0f}, true);
    ground.setBaseColor(glm::vec3{0.2f, 0.22f, 0.25f});

    const glm::vec3 colors[] = {
        {0.9f, 0.3f, 0.2f},
        {0.2f, 0.7f, 0.9f},
        {0.7f, 0.85f, 0.3f},
        {0.9f, 0.6f, 0.15f},
        {0.7f, 0.3f, 0.9f}
    };

    for (int i = 0; i < 5; ++i) {
        auto& cube = engine.createObject("Cube" + std::to_string(i), vkengine::MeshType::Cube);
        cube.transform().position = glm::vec3{-4.0f + i * 2.0f, 0.5f, -1.5f};
        cube.transform().scale = glm::vec3{1.2f};
        cube.setBaseColor(colors[i]);
    }

    auto& pillar = engine.createObject("Pillar", vkengine::MeshType::Cube);
    pillar.transform().position = glm::vec3{3.5f, 1.8f, -4.5f};
    pillar.transform().scale = glm::vec3{1.0f, 2.4f, 1.0f};
    pillar.setBaseColor(glm::vec3{0.85f, 0.85f, 0.9f});

    vkengine::LightCreateInfo keyLight{};
    keyLight.name = "KeyLight";
    keyLight.position = glm::vec3{2.5f, 5.5f, 4.0f};
    keyLight.color = glm::vec3{1.0f, 0.96f, 0.9f};
    keyLight.intensity = 2.5f;
    scene.createLight(keyLight);

    VulkanRenderer renderer(engine);
    renderer.setSkyColor(glm::vec4{0.08f, 0.09f, 0.12f, 1.0f});
    renderer.setLightAnimationEnabled(false);

    std::cout << "\n=== Shader Styles Example ===\n";
    std::cout << "Use F5 or the Scene Controls UI to cycle shader styles.\n";
    std::cout << "F1 cycles debug shading views, F2 toggles shadows.\n\n";

    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "ShaderStylesExample");
    if (capture.enabled) {
        WindowConfig window{};
        window.width = capture.width;
        window.height = capture.height;
        window.headless = true;
        window.resizable = false;
        renderer.setWindowConfig(window);
        renderer.setSceneControlsEnabled(false);
        std::filesystem::create_directories(capture.outputPath.parent_path());
        return renderer.renderSingleFrameToJpeg(capture.outputPath) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    renderer.run();
    return EXIT_SUCCESS;
} catch (const std::exception& e) {
    std::cerr << "Shader styles example failed: " << e.what() << '\n';
    return EXIT_FAILURE;
}
