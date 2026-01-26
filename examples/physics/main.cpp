#include "engine/GameEngine.hpp"
#include "engine/HeadlessCapture.hpp"
#include "VulkanRenderer.hpp"

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

    // Configure physics
    engine.physics().setGravity(glm::vec3{0.0f, -9.8f, 0.0f});

    // Setup camera to view the tower
    auto& camera = scene.camera();
    camera.setPosition(glm::vec3{8.0f, 5.0f, 12.0f});
    camera.lookAt(glm::vec3{0.0f, 3.0f, 0.0f});
    camera.setMovementSpeed(6.0f);

    // Create ground plane
    auto& ground = engine.createObject("Ground", vkengine::MeshType::Cube);
    ground.transform().scale = glm::vec3{12.0f, 0.3f, 12.0f};
    ground.transform().position = glm::vec3{0.0f, -1.5f, 0.0f};
    ground.enableCollider(glm::vec3{6.0f, 0.15f, 6.0f}, true);

    // Build a tower of small cubes (5 levels, 3x3 per level)
    const float cubeSize = 0.4f;
    const float cubeSpacing = 0.42f;
    const glm::vec3 cubeHalfExtents{cubeSize * 0.5f};
    const float groundTop = ground.transform().position.y + ground.transform().scale.y;

    std::cout << "\n=== Physics Tower Example ===\n";
    std::cout << "Building tower of cubes...\n";

    int cubeCount = 0;
    for (int level = 0; level < 5; ++level) {
        const float levelY = groundTop + cubeSize * 0.5f + level * cubeSize;
        
        for (int row = -1; row <= 1; ++row) {
            for (int col = -1; col <= 1; ++col) {
                auto& cube = engine.createObject("TowerCube_" + std::to_string(cubeCount++), 
                                                  vkengine::MeshType::Cube);
                cube.transform().position = glm::vec3{
                    col * cubeSpacing,
                    levelY,
                    row * cubeSpacing
                };
                cube.transform().scale = glm::vec3{cubeSize};
                
                // Enable physics and collision
                cube.physics().simulate = true;
                cube.physics().mass = 0.5f;
                cube.physics().restitution = 0.15f;
                cube.physics().staticFriction = 0.85f;
                cube.physics().dynamicFriction = 0.65f;
                cube.enableCollider(cubeHalfExtents);
            }
        }
    }

    std::cout << "Tower built with " << cubeCount << " cubes.\n";

    // Create the wrecking ball - spawned high above and to the side
    auto& ball = engine.createObject("WreckingBall", vkengine::MeshType::Cube);
    ball.transform().position = glm::vec3{-4.0f, 8.0f, 0.0f};
    ball.transform().scale = glm::vec3{0.8f};
    ball.physics().simulate = true;
    ball.physics().mass = 3.0f;
    ball.physics().velocity = glm::vec3{6.0f, 0.0f, 0.0f};  // Launch toward tower
    ball.physics().restitution = 0.65f;
    ball.physics().linearDamping = 0.01f;
    ball.physics().angularDamping = 0.02f;
    ball.physics().staticFriction = 0.6f;
    ball.physics().dynamicFriction = 0.5f;
    ball.physics().angularVelocity = glm::vec3{0.0f, 5.0f, 0.0f};
    ball.enableCollider(glm::vec3{0.4f});

    std::cout << "Wrecking ball launched!\n";

    // Add a few static barriers on the sides
    auto& leftWall = engine.createObject("LeftWall", vkengine::MeshType::Cube);
    leftWall.transform().position = glm::vec3{-6.0f, 1.0f, 0.0f};
    leftWall.transform().scale = glm::vec3{0.5f, 4.0f, 8.0f};
    leftWall.enableCollider(glm::vec3{0.25f, 2.0f, 4.0f}, true);

    auto& rightWall = engine.createObject("RightWall", vkengine::MeshType::Cube);
    rightWall.transform().position = glm::vec3{6.0f, 1.0f, 0.0f};
    rightWall.transform().scale = glm::vec3{0.5f, 4.0f, 8.0f};
    rightWall.enableCollider(glm::vec3{0.25f, 2.0f, 4.0f}, true);

    auto& backWall = engine.createObject("BackWall", vkengine::MeshType::Cube);
    backWall.transform().position = glm::vec3{0.0f, 1.0f, -6.0f};
    backWall.transform().scale = glm::vec3{8.0f, 4.0f, 0.5f};
    backWall.enableCollider(glm::vec3{4.0f, 2.0f, 0.25f}, true);

    // Lighting setup
    vkengine::LightCreateInfo keyLightInfo{};
    keyLightInfo.name = "KeyLight";
    keyLightInfo.position = glm::vec3{4.0f, 10.0f, 4.0f};
    keyLightInfo.color = glm::vec3{1.0f, 0.95f, 0.85f};
    keyLightInfo.intensity = 8.0f;
    scene.createLight(keyLightInfo);

    vkengine::LightCreateInfo fillLightInfo{};
    fillLightInfo.name = "FillLight";
    fillLightInfo.position = glm::vec3{-3.0f, 5.0f, 3.0f};
    fillLightInfo.color = glm::vec3{0.7f, 0.8f, 1.0f};
    fillLightInfo.intensity = 3.0f;
    scene.createLight(fillLightInfo);

    std::cout << "\nPhysics Configuration:\n";
    std::cout << "  - Gravity: " << engine.physics().getGravity().y << " m/s²\n";
    std::cout << "  - Tower cubes: " << cubeCount << " (mass: 0.5 kg each)\n";
    std::cout << "  - Wrecking ball: mass 3.0 kg, velocity 6.0 m/s\n";
    std::cout << "\nControls:\n";
    std::cout << "  W/A/S/D - Move camera\n";
    std::cout << "  Space/Ctrl - Move up/down\n";
    std::cout << "  Right Mouse - Look around\n";
    std::cout << "  F2 - Toggle shadows\n";
    std::cout << "  F4 - Toggle light animation\n\n";

    VulkanCubeApp renderer(engine);
    renderer.setLightAnimationEnabled(false);
    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "PhysicsExample");
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
    std::cerr << "Physics example failed: " << e.what() << '\n';
    return EXIT_FAILURE;
}
