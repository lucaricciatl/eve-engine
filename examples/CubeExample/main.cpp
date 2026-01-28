#include "CubeExample.hpp"
#include "engine/HeadlessCapture.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
    camera.setPosition(glm::vec3{3.0f, 2.5f, 3.0f});
    camera.setYawPitch(glm::radians(-135.0f), glm::radians(-20.0f));
    camera.setMovementSpeed(4.0f);

    auto& ground = engine.createObject("Ground", vkengine::MeshType::Cube);
    ground.transform().scale = glm::vec3{6.0f, 0.25f, 6.0f};
    ground.transform().position = glm::vec3{0.0f, -1.5f, 0.0f};
    ground.enableCollider(glm::vec3{3.0f, 0.125f, 3.0f}, true);
    ground.setMaterial("wood");

    auto& falling = engine.createObject("FallingCube", vkengine::MeshType::Cube);
    falling.transform().position = glm::vec3{0.0f, 1.25f, 0.0f};
    falling.physics().simulate = true;
    falling.physics().mass = 2.0f;
    falling.enableCollider(glm::vec3{0.5f, 0.5f, 0.5f});
    falling.setMaterial("steel");

    auto& drifting = engine.createObject("Drifter", vkengine::MeshType::Cube);
    drifting.transform().position = glm::vec3{-1.5f, 0.0f, 0.0f};
    drifting.physics().simulate = true;
    drifting.physics().mass = 1.0f;
    drifting.physics().velocity = glm::vec3{0.75f, 0.0f, 0.0f};
    drifting.enableCollider(glm::vec3{0.5f, 0.5f, 0.5f});
    drifting.setMaterial("glass");

    auto& imported = engine.createObject("ImportedMesh", vkengine::MeshType::CustomMesh);
    imported.transform().position = glm::vec3{1.5f, 0.25f, -1.5f};
    imported.transform().scale = glm::vec3{0.75f};
    imported.setMeshResource("assets/models/demo_cube.stl");
    imported.setMaterial("metal");
    imported.setAlbedoTexture("assets/textures/checker.ppm");

    vkengine::LightCreateInfo keyLightInfo{};
    keyLightInfo.name = "KeyLight";
    keyLightInfo.position = glm::vec3{2.0f, 4.0f, 2.0f};
    keyLightInfo.color = glm::vec3{1.0f, 0.95f, 0.85f};
    keyLightInfo.intensity = 2.0f;
    auto& keyLight = scene.createLight(keyLightInfo);

    VulkanCubeApp renderer(engine);
    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "CubeExample");
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
    std::cerr << "Fatal error: " << e.what() << '\n';
    return EXIT_FAILURE;
}
