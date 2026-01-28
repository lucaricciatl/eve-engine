#include "engine/GameEngine.hpp"
#include "engine/HeadlessCapture.hpp"
#include "core/VulkanRenderer.hpp"

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
    ground.setBaseColor(glm::vec3{0.92f, 0.92f, 0.95f});

    // Create several cubes to demonstrate lighting
    auto& centerCube = engine.createObject("CenterCube", vkengine::MeshType::Cube);
    centerCube.transform().position = glm::vec3{0.0f, 0.5f, 0.0f};
    centerCube.transform().scale = glm::vec3{1.2f};
    centerCube.setBaseColor(glm::vec3{0.9f, 0.9f, 0.9f});

    auto& leftCube = engine.createObject("LeftCube", vkengine::MeshType::Cube);
    leftCube.transform().position = glm::vec3{-3.0f, 0.5f, 0.0f};
    leftCube.setBaseColor(glm::vec3{0.82f, 0.84f, 0.88f});

    auto& rightCube = engine.createObject("RightCube", vkengine::MeshType::Cube);
    rightCube.transform().position = glm::vec3{3.0f, 0.5f, 0.0f};
    rightCube.setBaseColor(glm::vec3{0.88f, 0.82f, 0.84f});

    auto& backCube = engine.createObject("BackCube", vkengine::MeshType::Cube);
    backCube.transform().position = glm::vec3{0.0f, 0.5f, -3.0f};
    backCube.setBaseColor(glm::vec3{0.85f, 0.88f, 0.82f});

    // Example 1: Key area light (warm, overhead)
    vkengine::LightCreateInfo keyLightInfo{};
    keyLightInfo.name = "KeyLight";
    keyLightInfo.position = glm::vec3{2.0f, 5.0f, 3.0f};
    keyLightInfo.direction = glm::normalize(glm::vec3{-0.3f, -1.0f, -0.2f});
    keyLightInfo.type = vkengine::LightType::Area;
    keyLightInfo.areaSize = glm::vec2{3.0f, 2.0f};
    keyLightInfo.range = 22.0f;
    keyLightInfo.color = glm::vec3{1.0f, 0.95f, 0.85f};  // Warm white
    keyLightInfo.intensity = 1.4f;
    keyLightInfo.enabled = true;
    auto& keyLight = scene.createLight(keyLightInfo);

    // Example 2: Fill point light (cooler, dimmer, opposite side)
    vkengine::LightCreateInfo fillLightInfo{};
    fillLightInfo.name = "FillLight";
    fillLightInfo.position = glm::vec3{-3.0f, 3.0f, 2.0f};
    fillLightInfo.type = vkengine::LightType::Point;
    fillLightInfo.range = 16.0f;
    fillLightInfo.color = glm::vec3{0.6f, 0.7f, 1.0f};  // Cool blue-ish
    fillLightInfo.intensity = 0.8f;
    auto& fillLight = scene.createLight(fillLightInfo);

    // Example 3: Rim/back spot light (creates edge definition)
    vkengine::LightCreateInfo rimLightInfo{};
    rimLightInfo.name = "RimLight";
    rimLightInfo.position = glm::vec3{0.0f, 4.0f, -5.0f};
    rimLightInfo.direction = glm::normalize(glm::vec3{0.0f, -0.4f, 1.0f});
    rimLightInfo.type = vkengine::LightType::Spot;
    rimLightInfo.innerConeAngle = 0.174533f;
    rimLightInfo.outerConeAngle = 0.418879f;
    rimLightInfo.range = 20.0f;
    rimLightInfo.color = glm::vec3{1.0f, 0.9f, 0.7f};
    rimLightInfo.intensity = 0.9f;
    auto& rimLight = scene.createLight(rimLightInfo);

    // Example 4: Accent point light (colored, low position)
    vkengine::LightCreateInfo accentLightInfo{};
    accentLightInfo.name = "AccentLight";
    accentLightInfo.position = glm::vec3{4.0f, 1.0f, 1.0f};
    accentLightInfo.type = vkengine::LightType::Point;
    accentLightInfo.range = 14.0f;
    accentLightInfo.color = glm::vec3{1.0f, 0.3f, 0.4f};  // Pinkish red
    accentLightInfo.intensity = 0.6f;
    auto& accentLight = scene.createLight(accentLightInfo);

    // Example 5: Traditional approach - create light with name, then configure
    auto& manualLight = scene.createLight("ManualLight");
    manualLight.setPosition(glm::vec3{-4.0f, 2.0f, -2.0f});
    manualLight.setColor(glm::vec3{0.4f, 1.0f, 0.5f});  // Greenish
    manualLight.setIntensity(0.5f);
    manualLight.setEnabled(true);

    std::cout << "\n=== Lights Example ===\n";
    std::cout << "This demo showcases multiple lighting configurations:\n";
    std::cout << "  - KeyLight: Warm overhead area light\n";
    std::cout << "  - FillLight: Cool point light (opposite side)\n";
    std::cout << "  - RimLight: Spot light for edge definition\n";
    std::cout << "  - AccentLight: Colored point accent\n";
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
