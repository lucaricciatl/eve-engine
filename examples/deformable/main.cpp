#include "engine/GameEngine.hpp"
#include "engine/HeadlessCapture.hpp"
#include "core/VulkanRenderer.hpp"

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
    camera.setPosition(glm::vec3{0.0f, 2.5f, 8.0f});
    camera.lookAt(glm::vec3{0.0f, 1.0f, 0.0f});
    camera.setMovementSpeed(6.0f);

    auto& ground = engine.createObject("Ground", vkengine::MeshType::Cube);
    ground.transform().scale = glm::vec3{8.0f, 0.2f, 8.0f};
    ground.transform().position = glm::vec3{0.0f, -2.0f, 0.0f};
    ground.enableCollider(glm::vec3{4.0f, 0.1f, 4.0f}, true);
    const float groundTopY = ground.transform().position.y + ground.transform().scale.y;

    auto& clothObject = engine.createObject("SoftCloth", vkengine::MeshType::DeformableCloth);
    clothObject.transform().position = glm::vec3{-2.5f, 3.0f, 0.0f};
    clothObject.transform().scale = glm::vec3{1.0f};

    auto& cloth = clothObject.enableDeformableCloth(24, 16, 0.2f);
    cloth.pinTopEdge();
    cloth.addCentralImpulse(glm::vec3{0.0f, 3.0f, 0.0f});

    auto& jellyObject = engine.createObject("JellyCube", vkengine::MeshType::SoftBodyVolume);
    jellyObject.transform().position = glm::vec3{2.0f, 0.75f, 0.0f};
    jellyObject.transform().scale = glm::vec3{1.0f};

    auto& jelly = jellyObject.enableSoftBodyVolume(6, 6, 6, 0.35f);
    jelly.setStiffness(320.0f);
    jelly.setDamping(0.18f);
    constexpr float surfacePadding = 0.02f;
    const float localFloorY = (groundTopY + surfacePadding) - jellyObject.transform().position.y;
    jelly.setFloorY(localFloorY);
    jelly.addImpulse(glm::vec3{0.0f, 5.0f, 0.0f});

    vkengine::LightCreateInfo rigLightInfo{};
    rigLightInfo.name = "DeformableLight";
    rigLightInfo.position = glm::vec3{-6.0f, 10.0f, 4.5f};
    rigLightInfo.color = glm::vec3{1.0f, 0.97f, 0.92f};
    rigLightInfo.intensity = 6.0f;
    auto& light = scene.createLight(rigLightInfo);

    VulkanRenderer renderer(engine);
    renderer.setLightAnimationEnabled(false);
    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "DeformableExample");
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
    std::cerr << "Deformable example failed: " << e.what() << '\n';
    return EXIT_FAILURE;
}
