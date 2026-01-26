#include "engine/GameEngine.hpp"
#include "engine/HeadlessCapture.hpp"
#include "VulkanRenderer.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv)
try {
    vkengine::GameEngine engine;

    auto& scene = engine.scene();
    scene.clear();

    // Setup camera to view the scene
    auto& camera = scene.camera();
    camera.setPosition(glm::vec3{0.0f, 4.0f, 12.0f});
    camera.lookAt(glm::vec3{0.0f, 0.5f, 0.0f});
    camera.setMovementSpeed(6.0f);

    // Create a ground plane
    auto& ground = engine.createObject("Ground", vkengine::MeshType::Cube);
    ground.transform().scale = glm::vec3{10.0f, 0.2f, 10.0f};
    ground.transform().position = glm::vec3{0.0f, -1.5f, 0.0f};
    ground.enableCollider(glm::vec3{5.0f, 0.1f, 5.0f}, true);

    // Create several cubes to demonstrate lighting
    auto& centerCube = engine.createObject("CenterCube", vkengine::MeshType::Cube);
    centerCube.transform().position = glm::vec3{0.0f, 0.5f, 0.0f};
    centerCube.transform().scale = glm::vec3{1.2f};

    auto& leftCube = engine.createObject("LeftCube", vkengine::MeshType::Cube);
    leftCube.transform().position = glm::vec3{-3.0f, 0.5f, 0.0f};

    auto& rightCube = engine.createObject("RightCube", vkengine::MeshType::Cube);
    rightCube.transform().position = glm::vec3{3.0f, 0.5f, 0.0f};

    auto& backCube = engine.createObject("BackCube", vkengine::MeshType::Cube);
    backCube.transform().position = glm::vec3{0.0f, 0.5f, -3.0f};

    // Example 1: Key light using LightCreateInfo helper (warm, overhead)
    vkengine::LightCreateInfo keyLightInfo{};
    keyLightInfo.name = "KeyLight";
    keyLightInfo.position = glm::vec3{2.0f, 5.0f, 3.0f};
    keyLightInfo.color = glm::vec3{1.0f, 0.95f, 0.85f};  // Warm white
    keyLightInfo.intensity = 5.0f;
    keyLightInfo.enabled = true;
    auto& keyLight = scene.createLight(keyLightInfo);

    // Example 2: Fill light (cooler, dimmer, opposite side)
    vkengine::LightCreateInfo fillLightInfo{};
    fillLightInfo.name = "FillLight";
    fillLightInfo.position = glm::vec3{-3.0f, 3.0f, 2.0f};
    fillLightInfo.color = glm::vec3{0.6f, 0.7f, 1.0f};  // Cool blue-ish
    fillLightInfo.intensity = 2.5f;
    auto& fillLight = scene.createLight(fillLightInfo);

    // Example 3: Rim/back light (creates edge definition)
    vkengine::LightCreateInfo rimLightInfo{};
    rimLightInfo.name = "RimLight";
    rimLightInfo.position = glm::vec3{0.0f, 4.0f, -5.0f};
    rimLightInfo.color = glm::vec3{1.0f, 0.9f, 0.7f};
    rimLightInfo.intensity = 3.0f;
    auto& rimLight = scene.createLight(rimLightInfo);

    // Example 4: Accent light (colored, low position)
    vkengine::LightCreateInfo accentLightInfo{};
    accentLightInfo.name = "AccentLight";
    accentLightInfo.position = glm::vec3{4.0f, 1.0f, 1.0f};
    accentLightInfo.color = glm::vec3{1.0f, 0.3f, 0.4f};  // Pinkish red
    accentLightInfo.intensity = 2.0f;
    auto& accentLight = scene.createLight(accentLightInfo);

    // Example 5: Traditional approach - create light with name, then configure
    auto& manualLight = scene.createLight("ManualLight");
    manualLight.setPosition(glm::vec3{-4.0f, 2.0f, -2.0f});
    manualLight.setColor(glm::vec3{0.4f, 1.0f, 0.5f});  // Greenish
    manualLight.setIntensity(1.8f);
    manualLight.setEnabled(true);

    std::cout << "\n=== Lights Example ===\n";
    std::cout << "This demo showcases multiple lighting configurations:\n";
    std::cout << "  - KeyLight: Main warm overhead light\n";
    std::cout << "  - FillLight: Cool secondary light (opposite side)\n";
    std::cout << "  - RimLight: Back light for edge definition\n";
    std::cout << "  - AccentLight: Colored accent from the side\n";
    std::cout << "  - ManualLight: Traditional setter-based configuration\n";
    std::cout << "\nControls:\n";
    std::cout << "  W/A/S/D - Move camera\n";
    std::cout << "  Space/Ctrl - Move up/down\n";
    std::cout << "  Right Mouse - Look around\n";
    std::cout << "  F2 - Toggle shadows\n";
    std::cout << "  F4 - Toggle light animation\n\n";

    VulkanCubeApp renderer(engine);
    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "LightsExample");
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
    std::cerr << "Lights example failed: " << e.what() << '\n';
    return EXIT_FAILURE;
}
