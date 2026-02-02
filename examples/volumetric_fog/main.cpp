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
    camera.setPosition(glm::vec3{0.0f, 2.2f, 12.0f});
    camera.lookAt(glm::vec3{0.0f, 0.8f, 0.0f});
    camera.setMovementSpeed(6.0f);

    auto& ground = engine.createObject("Ground", vkengine::MeshType::Cube);
    ground.transform().scale = glm::vec3{14.0f, 0.2f, 14.0f};
    ground.transform().position = glm::vec3{0.0f, -1.5f, 0.0f};
    ground.enableCollider(glm::vec3{7.0f, 0.1f, 7.0f}, true);
    ground.setBaseColor(glm::vec3{0.2f, 0.2f, 0.25f});

    const int columnCount = 9;
    for (int i = 0; i < columnCount; ++i) {
        const float z = -static_cast<float>(i) * 4.0f;
        const float x = (i % 2 == 0) ? -2.2f : 2.2f;
        auto& column = engine.createObject("Column" + std::to_string(i), vkengine::MeshType::Cube);
        column.transform().scale = glm::vec3{1.0f, 2.2f + i * 0.25f, 1.0f};
        column.transform().position = glm::vec3{x, 0.4f, z};
        column.setBaseColor(glm::vec3{0.75f - i * 0.05f, 0.72f - i * 0.05f, 0.85f});
    }

    auto& center = engine.createObject("Center", vkengine::MeshType::Cube);
    center.transform().position = glm::vec3{0.0f, 0.6f, -2.0f};
    center.transform().scale = glm::vec3{1.4f};
    center.setBaseColor(glm::vec3{0.85f, 0.65f, 0.3f});

    auto& wall = engine.createObject("Backdrop", vkengine::MeshType::Cube);
    wall.transform().position = glm::vec3{0.0f, 1.5f, -18.0f};
    wall.transform().scale = glm::vec3{18.0f, 6.0f, 0.4f};
    wall.setBaseColor(glm::vec3{0.25f, 0.2f, 0.2f});

    vkengine::LightCreateInfo keyLight{};
    keyLight.name = "KeyLight";
    keyLight.position = glm::vec3{2.5f, 5.5f, 4.0f};
    keyLight.color = glm::vec3{1.0f, 0.96f, 0.9f};
    keyLight.intensity = 2.5f;
    scene.createLight(keyLight);

    VulkanRenderer renderer(engine);
    renderer.setSkyColor(glm::vec4{0.08f, 0.09f, 0.12f, 1.0f});

    VulkanRenderer::FogSettings fog{};
    fog.enabled = true;
    fog.color = glm::vec3{0.95f, 0.15f, 0.15f};
    fog.density = 0.2f;
    fog.heightFalloff = 0.12f;
    fog.heightOffset = -0.6f;
    fog.startDistance = 0.0f;
    renderer.setFogSettings(fog);
    renderer.setLightAnimationEnabled(false);

    std::cout << "\n=== Volumetric Fog Example ===\n";
    std::cout << "Demonstrates height-based volumetric fog with distance falloff.\n";
    std::cout << "Use the Scene Controls UI to tweak fog density and color.\n\n";

    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "VolumetricFogExample");
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
    std::cerr << "Volumetric fog example failed: " << e.what() << '\n';
    return EXIT_FAILURE;
}
