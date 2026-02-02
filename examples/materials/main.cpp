#include "engine/GameEngine.hpp"
#include "engine/HeadlessCapture.hpp"
#include "core/VulkanRenderer.hpp"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
try {
    vkengine::GameEngine engine;

    auto& scene = engine.scene();
    scene.clear();

    auto& camera = scene.camera();
    camera.setPosition(glm::vec3{0.0f, 4.0f, 10.0f});
    camera.lookAt(glm::vec3{0.0f, 0.5f, 0.0f});
    camera.setMovementSpeed(5.0f);

    auto& ground = engine.createObject("Ground", vkengine::MeshType::Cube);
    ground.transform().scale = glm::vec3{12.0f, 0.25f, 12.0f};
    ground.transform().position = glm::vec3{0.0f, -1.5f, 0.0f};
    ground.enableCollider(glm::vec3{6.0f, 0.125f, 6.0f}, true);
    ground.setMaterial("concrete");

    vkengine::Material brushed{};
    brushed.name = "brushed_steel";
    brushed.baseColor = glm::vec4(0.68f, 0.7f, 0.75f, 1.0f);
    brushed.metallic = 0.85f;
    brushed.roughness = 0.35f;
    brushed.specular = 0.8f;
    scene.materials().add(brushed);

    vkengine::Material neon{};
    neon.name = "neon";
    neon.baseColor = glm::vec4(0.05f, 0.05f, 0.08f, 1.0f);
    neon.metallic = 0.0f;
    neon.roughness = 0.2f;
    neon.specular = 0.6f;
    neon.emissive = glm::vec3(0.2f, 0.9f, 0.8f);
    neon.emissiveIntensity = 2.5f;
    scene.materials().add(neon);

    vkengine::Material scifiPanel{};
    scifiPanel.name = "scifi_panel";
    scifiPanel.baseColor = glm::vec4(0.22f, 0.26f, 0.32f, 1.0f);
    scifiPanel.metallic = 0.6f;
    scifiPanel.roughness = 0.35f;
    scifiPanel.specular = 0.75f;
    scene.materials().add(scifiPanel);

    vkengine::Material scifiGlow{};
    scifiGlow.name = "scifi_glow";
    scifiGlow.baseColor = glm::vec4(0.08f, 0.1f, 0.14f, 1.0f);
    scifiGlow.metallic = 0.1f;
    scifiGlow.roughness = 0.2f;
    scifiGlow.specular = 0.7f;
    scifiGlow.emissive = glm::vec3(0.0f, 0.65f, 1.0f);
    scifiGlow.emissiveIntensity = 3.0f;
    scene.materials().add(scifiGlow);

    vkengine::Material moss{};
    moss.name = "natural_moss";
    moss.baseColor = glm::vec4(0.23f, 0.45f, 0.2f, 1.0f);
    moss.metallic = 0.0f;
    moss.roughness = 0.9f;
    moss.specular = 0.15f;
    scene.materials().add(moss);

    vkengine::Material sand{};
    sand.name = "natural_sand";
    sand.baseColor = glm::vec4(0.82f, 0.76f, 0.6f, 1.0f);
    sand.metallic = 0.0f;
    sand.roughness = 0.95f;
    sand.specular = 0.1f;
    scene.materials().add(sand);

    std::vector<std::string> names = scene.materials().names();
    std::sort(names.begin(), names.end());

    const std::size_t columns = 4;
    const float spacing = 2.0f;
    const float startX = -static_cast<float>((columns - 1) * 0.5f) * spacing;
    const float startZ = 2.5f;

    for (std::size_t i = 0; i < names.size(); ++i) {
        const std::size_t col = i % columns;
        const std::size_t row = i / columns;
        const float x = startX + static_cast<float>(col) * spacing;
        const float z = startZ - static_cast<float>(row) * spacing;

        auto& object = engine.createObject(names[i], vkengine::MeshType::Cube);
        object.transform().position = glm::vec3{x, 0.5f, z};
        object.transform().scale = glm::vec3{0.85f};
        object.setMaterial(names[i]);
    }

    vkengine::LightCreateInfo keyLight{};
    keyLight.name = "KeyLight";
    keyLight.position = glm::vec3{4.0f, 6.0f, 4.0f};
    keyLight.color = glm::vec3{1.0f, 0.96f, 0.9f};
    keyLight.intensity = 2.5f;
    scene.createLight(keyLight);

    vkengine::LightCreateInfo fillLight{};
    fillLight.name = "Fill";
    fillLight.position = glm::vec3{-6.0f, 3.0f, 1.0f};
    fillLight.color = glm::vec3{0.7f, 0.85f, 1.0f};
    fillLight.intensity = 1.0f;
    scene.createLight(fillLight);

    std::cout << "\n=== Materials Example ===\n";
    std::cout << "Shows every built-in material in a grid.\n\n";

    VulkanRenderer renderer(engine);
    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "MaterialsExample");
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
    std::cerr << "Materials example failed: " << e.what() << '\n';
    return EXIT_FAILURE;
}
