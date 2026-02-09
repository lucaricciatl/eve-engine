// GPU Collision Detection Example
// Demonstrates the Vulkan compute shader-based collision detection system
// Based on GPU Gems 3, Chapter 32: Broad-Phase Collision Detection with CUDA
// Ported to Vulkan compute shaders for cross-platform support

#include "core/VulkanRenderer.hpp"
#include "engine/GameEngine.hpp"
#include "engine/GpuCollisionSystem.hpp"
#include "engine/HeadlessCapture.hpp"

#include <glm/glm.hpp>

#include <cstdlib>
#include <iostream>
#include <random>
#include <string>

int main(int argc, char** argv)
{
    try {
        vkengine::GameEngine engine;
        VulkanRenderer renderer(engine);

        // Configure window
        WindowConfig windowConfig{};
        windowConfig.title = "GPU Collision Detection Demo";
        windowConfig.width = 1280;
        windowConfig.height = 720;
        renderer.setWindowConfig(windowConfig);

        auto& scene = engine.scene();
        
        // Setup camera to view the collision scene
        auto& camera = scene.camera();
        camera.setPosition(glm::vec3{15.0f, 10.0f, 15.0f});
        camera.lookAt(glm::vec3{0.0f, 2.0f, 0.0f});
        camera.setMovementSpeed(8.0f);

        // Create ground plane (static collider)
        // Ground is at y=0, with top surface at y=0.5
        auto& ground = engine.createObject("Ground", vkengine::MeshType::Cube);
        ground.transform().position = glm::vec3{0.0f, 0.0f, 0.0f};
        ground.transform().scale = glm::vec3{20.0f, 1.0f, 20.0f};
        // Collider half-extents = scale * 0.5 (since unit cube has half-extent of 0.5)
        ground.enableCollider(glm::vec3{10.0f, 0.5f, 10.0f}, true);
        ground.setBaseColor(glm::vec3{0.4f, 0.5f, 0.4f});

        // Random number generator for spawning objects
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> posDist(-8.0f, 8.0f);
        std::uniform_real_distribution<float> heightDist(2.0f, 15.0f);
        std::uniform_real_distribution<float> colorDist(0.3f, 1.0f);
        std::uniform_real_distribution<float> scaleDist(0.3f, 0.8f);
        std::uniform_real_distribution<float> rotationDist(0.0f, 6.283185f); // 0 to 2*PI

        // Create dynamic cubes for collision test
        constexpr int NUM_CUBES = 30;
        std::cout << "Creating " << NUM_CUBES << " dynamic cubes for GPU collision test...\n";
        
        // Ground top surface is at y = 0.5
        const float groundTop = 0.5f;

        for (int i = 0; i < NUM_CUBES; ++i) {
            auto& cube = engine.createObject("Cube_" + std::to_string(i), vkengine::MeshType::Cube);
            
            float scale = scaleDist(gen);
            float halfExtent = scale * 0.5f;
            
            // Spawn cubes above ground with spacing to avoid initial overlap
            cube.transform().position = glm::vec3{
                posDist(gen),
                groundTop + halfExtent + 1.0f + i * 1.0f,  // Stack vertically above ground
                posDist(gen)
            };
            cube.transform().scale = glm::vec3{scale};
            
            // Random initial rotation on all axes
            cube.transform().rotation = glm::vec3{
                rotationDist(gen),
                rotationDist(gen),
                rotationDist(gen)
            };
            
            // Enable physics simulation with stable parameters
            cube.physics().simulate = true;
            cube.physics().mass = scale * scale * scale * 2.0f; // Mass proportional to volume
            cube.physics().restitution = 0.1f;       // Low bounce for stability
            cube.physics().linearDamping = 0.05f;    // Light damping
            cube.physics().angularDamping = 0.1f;    // Light angular damping
            cube.physics().staticFriction = 0.6f;
            cube.physics().dynamicFriction = 0.5f;
            
            // Enable collision - half extents = scale * 0.5 (unit cube has half-extent 0.5)
            cube.enableCollider(glm::vec3{halfExtent});
            
            // Random color
            cube.setBaseColor(glm::vec3{
                colorDist(gen),
                colorDist(gen),
                colorDist(gen)
            });
        }

        std::cout << "GPU Collision Detection Demo\n";
        std::cout << "============================\n";
        std::cout << "This demo showcases the Vulkan compute shader-based\n";
        std::cout << "collision detection system, implementing:\n";
        std::cout << "  - Spatial hashing for broad-phase\n";
        std::cout << "  - AABB intersection for narrow-phase\n";
        std::cout << "  - Parallel prefix sum and bitonic sort on GPU\n\n";
        std::cout << "Press 'G' to toggle GPU collision detection\n";
        std::cout << "Press 'H' for help with other controls\n\n";

        // Note: GPU collision system needs to be initialized by the renderer
        // after Vulkan device is created. The physics system will automatically
        // use it when enabled.
        
        // For now, we run with CPU collision (GPU requires renderer integration)
        // To enable GPU collision:
        engine.physics().setGpuCollisionEnabled(true);

        // Handle headless capture for automated testing
        const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "GpuCollisionExample");
        if (capture.enabled) {
            renderer.renderSingleFrameToJpeg(capture.outputPath);
            return EXIT_SUCCESS;
        }

        renderer.run();
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
