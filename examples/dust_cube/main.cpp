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
    camera.setPosition(glm::vec3{2.5f, 2.2f, 3.5f});
    camera.lookAt(glm::vec3{0.0f, 0.5f, 0.0f});
    camera.setMovementSpeed(4.0f);

    auto& ground = engine.createObject("Ground", vkengine::MeshType::Cube);
    ground.transform().scale = glm::vec3{6.0f, 0.25f, 6.0f};
    ground.transform().position = glm::vec3{0.0f, -1.5f, 0.0f};
    ground.enableCollider(glm::vec3{3.0f, 0.125f, 3.0f}, true);
    ground.setMaterial("concrete");

    auto& cube = engine.createObject("DustCube", vkengine::MeshType::Cube);
    cube.transform().position = glm::vec3{-1.2f, 0.5f, 0.2f};
    cube.transform().scale = glm::vec3{1.2f};
    cube.setAlbedoTexture("assets/textures/dust-texture.svg");
    cube.setBaseColor(glm::vec3{0.95f, 0.93f, 0.9f});
    cube.render().roughness = 0.75f;
    cube.render().specular = 0.2f;

    auto& mixedCube = engine.createObject("DustCubeMixed", vkengine::MeshType::Cube);
    mixedCube.transform().position = glm::vec3{1.4f, 0.5f, -0.4f};
    mixedCube.transform().scale = glm::vec3{1.0f};
    mixedCube.setMaterial("steel");
    mixedCube.setAlbedoTexture("assets/textures/dust-texture.svg");
    mixedCube.setBaseColor(glm::vec3{0.85f, 0.88f, 0.92f});
    mixedCube.render().roughness = 0.45f;
    mixedCube.render().specular = 0.6f;
    
    vkengine::LightCreateInfo keyLight{};
    keyLight.name = "KeyLight";
    keyLight.position = glm::vec3{2.0f, 4.0f, 2.0f};
    keyLight.color = glm::vec3{1.0f, 0.95f, 0.9f};
    keyLight.intensity = 2.0f;
    scene.createLight(keyLight);

    VulkanCubeApp renderer(engine);
    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "DustCubeExample");
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
    std::cerr << "DustCube example failed: " << e.what() << '\n';
    return EXIT_FAILURE;
}
