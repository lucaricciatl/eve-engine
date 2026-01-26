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

    auto& camera = scene.camera();
    camera.setPosition(glm::vec3{0.0f, 2.25f, 8.0f});
    camera.lookAt(glm::vec3{0.0f, 0.5f, 0.0f});
    camera.setMovementSpeed(5.0f);

    auto& ground = engine.createObject("Ground", vkengine::MeshType::Cube);
    ground.transform().scale = glm::vec3{10.0f, 0.25f, 10.0f};
    ground.transform().position = glm::vec3{0.0f, -1.5f, 0.0f};
    ground.setBaseColor(glm::vec3{0.25f, 0.25f, 0.3f});
    ground.enableCollider(glm::vec3{5.0f, 0.125f, 5.0f}, true);

    auto& backWall = engine.createObject("Backdrop", vkengine::MeshType::Cube);
    backWall.transform().scale = glm::vec3{8.0f, 4.0f, 0.25f};
    backWall.transform().position = glm::vec3{0.0f, 1.0f, -4.0f};
    backWall.setBaseColor(glm::vec3{0.15f, 0.2f, 0.25f});

    auto& solidAccent = engine.createObject("Accent", vkengine::MeshType::Cube);
    solidAccent.transform().position = glm::vec3{0.0f, 0.5f, -1.75f};
    solidAccent.transform().scale = glm::vec3{1.2f, 1.2f, 1.2f};
    solidAccent.setBaseColor(glm::vec3{0.95f, 0.45f, 0.25f});

    auto& glassCenter = engine.createObject("GlassCenter", vkengine::MeshType::CustomMesh);
    glassCenter.transform().position = glm::vec3{0.0f, 0.6f, 0.0f};
    glassCenter.transform().scale = glm::vec3{1.2f, 1.2f, 1.2f};
    glassCenter.setMeshResource("assets/models/demo_cube.stl");
    glassCenter.setBaseColor(glm::vec4{0.6f, 0.8f, 1.0f, 0.25f});

    auto& glassLeft = engine.createObject("GlassLeft", vkengine::MeshType::CustomMesh);
    glassLeft.transform().position = glm::vec3{-2.5f, 0.4f, 0.5f};
    glassLeft.transform().scale = glm::vec3{0.9f, 0.9f, 0.9f};
    glassLeft.setMeshResource("assets/models/demo_cube.stl");
    glassLeft.setBaseColor(glm::vec4{0.45f, 0.9f, 0.7f, 0.2f});

    auto& glassRight = engine.createObject("GlassRight", vkengine::MeshType::CustomMesh);
    glassRight.transform().position = glm::vec3{2.5f, 0.7f, 0.3f};
    glassRight.transform().scale = glm::vec3{1.0f, 1.0f, 1.0f};
    glassRight.setMeshResource("assets/models/demo_cube.stl");
    glassRight.setBaseColor(glm::vec4{0.9f, 0.7f, 1.0f, 0.22f});

    vkengine::LightCreateInfo keyLight{};
    keyLight.name = "KeyLight";
    keyLight.position = glm::vec3{3.0f, 4.5f, 3.0f};
    keyLight.color = glm::vec3{1.0f, 0.96f, 0.9f};
    keyLight.intensity = 5.0f;
    scene.createLight(keyLight);

    vkengine::LightCreateInfo rimLight{};
    rimLight.name = "RimLight";
    rimLight.position = glm::vec3{-4.0f, 2.5f, -2.5f};
    rimLight.color = glm::vec3{0.6f, 0.8f, 1.0f};
    rimLight.intensity = 2.5f;
    scene.createLight(rimLight);

    std::cout << "\n=== Glass Example ===\n";
    std::cout << "Transparent cubes use a fresnel-tinted shader variant.\n";
    std::cout << "Try toggling specular with F3 and shadows with F2.\n\n";

    VulkanCubeApp renderer(engine);
    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "GlassExample");
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
    std::cerr << "Glass example failed: " << e.what() << '\n';
    return EXIT_FAILURE;
}