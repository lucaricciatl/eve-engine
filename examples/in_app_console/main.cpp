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
    camera.setPosition(glm::vec3{0.0f, 2.0f, 8.0f});
    camera.lookAt(glm::vec3{0.0f, 0.8f, 0.0f});
    camera.setMovementSpeed(5.0f);

    auto& ground = engine.createObject("Ground", vkengine::MeshType::Cube);
    ground.transform().scale = glm::vec3{10.0f, 0.2f, 10.0f};
    ground.transform().position = glm::vec3{0.0f, -1.2f, 0.0f};
    ground.enableCollider(glm::vec3{5.0f, 0.1f, 5.0f}, true);
    ground.setBaseColor(glm::vec3{0.25f, 0.25f, 0.28f});

    auto& cube = engine.createObject("Cube", vkengine::MeshType::Cube);
    cube.transform().position = glm::vec3{0.0f, 0.5f, 0.0f};
    cube.transform().scale = glm::vec3{1.4f};
    cube.setBaseColor(glm::vec3{0.8f, 0.6f, 0.2f});

    vkengine::LightCreateInfo keyLight{};
    keyLight.name = "KeyLight";
    keyLight.position = glm::vec3{2.5f, 4.5f, 3.0f};
    keyLight.color = glm::vec3{1.0f, 0.96f, 0.9f};
    keyLight.intensity = 2.0f;
    scene.createLight(keyLight);

    VulkanCubeApp renderer(engine);
    renderer.setSkyColor(glm::vec4{0.07f, 0.08f, 0.1f, 1.0f});
    renderer.setLightAnimationEnabled(false);
    renderer.setConsoleEnabled(true);
    renderer.setConsolePrompt("console");
    renderer.addConsoleLine("Welcome to the Vulkan console overlay.");
    renderer.addConsoleLine("Type in the input box and press Enter.");

    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "InAppConsoleExample");
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
    std::cerr << "In-app console example failed: " << e.what() << '\n';
    return EXIT_FAILURE;
}
