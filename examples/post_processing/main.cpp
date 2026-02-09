/**
 * @file main.cpp
 * @brief Visual Effects Demo
 * 
 * Demonstrates the visual effects currently implemented in the renderer:
 * - Fog (volumetric-style height fog with configurable density)
 * - Shader Styles (Standard, Toon, Rim, Heat, Gradient, Wireframe, Glow)
 * - Shadows (shadow mapping)
 * - Visualization Modes (Final Lit, Albedo, Normals, Shadow Factor, Fog Factor)
 * 
 * Use the ImGui controls to adjust each effect in real-time.
 */

#include "engine/GameEngine.hpp"
#include "engine/HeadlessCapture.hpp"
#include "core/VulkanRenderer.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <array>

// Effect presets
struct EffectPreset {
    const char* name;
    VulkanRenderer::ShaderStyle shaderStyle;
    VulkanRenderer::FogSettings fog;
};

static const std::array<EffectPreset, 6> kPresets = {{
    {"Default", VulkanRenderer::ShaderStyle::Standard, 
        {false, {0.65f, 0.7f, 0.75f}, 0.045f, 0.15f, 0.0f, 0.0f}},
    {"Foggy Morning", VulkanRenderer::ShaderStyle::Standard, 
        {true, {0.8f, 0.85f, 0.9f}, 0.08f, 0.2f, -2.0f, 0.0f}},
    {"Sunset Haze", VulkanRenderer::ShaderStyle::Standard, 
        {true, {0.95f, 0.6f, 0.4f}, 0.03f, 0.1f, 0.0f, 5.0f}},
    {"Toon Style", VulkanRenderer::ShaderStyle::Toon, 
        {false, {0.65f, 0.7f, 0.75f}, 0.045f, 0.15f, 0.0f, 0.0f}},
    {"Neon Glow", VulkanRenderer::ShaderStyle::Glow, 
        {true, {0.1f, 0.05f, 0.15f}, 0.02f, 0.3f, 0.0f, 0.0f}},
    {"Blueprint", VulkanRenderer::ShaderStyle::Wireframe, 
        {false, {0.65f, 0.7f, 0.75f}, 0.045f, 0.15f, 0.0f, 0.0f}},
}};

// Shader style names for UI
static const char* kShaderStyleNames[] = {
    "Standard", "Toon", "Rim Light", "Heat Map", "Gradient", "Wireframe", "Glow"
};

void setupScene(vkengine::GameEngine& engine) {
    auto& scene = engine.scene();
    scene.clear();
    
    // Set up camera
    auto& camera = scene.camera();
    camera.setPosition(glm::vec3{8.0f, 5.0f, 8.0f});
    camera.setYawPitch(glm::radians(-135.0f), glm::radians(-20.0f));
    camera.setMovementSpeed(5.0f);
    
    // Create ground plane (flat cube)
    auto& ground = engine.createObject("ground", vkengine::MeshType::Cube);
    ground.transform().position = glm::vec3{0.0f, -0.5f, 0.0f};
    ground.transform().scale = glm::vec3{20.0f, 0.1f, 20.0f};
    ground.setMaterial("stone");
    
    // Create a grid of cubes for visual interest
    const int gridSize = 4;
    const float spacing = 2.0f;
    const float offset = -spacing * (gridSize - 1) / 2.0f;
    
    // Material names
    const char* materials[] = {
        "metal", "wood", "glass", "steel", "copper", "gold", "plastic", "rubber", "stone"
    };
    
    int objectIndex = 0;
    for (int z = 0; z < gridSize; ++z) {
        for (int x = 0; x < gridSize; ++x) {
            std::string name = "cube_" + std::to_string(objectIndex);
            
            auto& obj = engine.createObject(name, vkengine::MeshType::Cube);
            obj.transform().position = glm::vec3{
                offset + x * spacing,
                0.5f + std::sin(objectIndex * 0.5f) * 0.5f,  // Varying heights
                offset + z * spacing
            };
            
            // Varying scales for visual interest
            float scaleY = 0.5f + (objectIndex % 5) * 0.25f;
            float scaleXZ = 0.6f + (objectIndex % 3) * 0.15f;
            obj.transform().scale = glm::vec3{scaleXZ, scaleY, scaleXZ};
            
            // Add some rotation for visual interest
            obj.transform().rotation = glm::vec3{0.0f, objectIndex * 22.5f, 0.0f};
            
            obj.setMaterial(materials[objectIndex % 9]);
            
            objectIndex++;
        }
    }
    
    // Add some taller pillars in the back for fog demonstration
    for (int i = 0; i < 5; ++i) {
        std::string name = "pillar_" + std::to_string(i);
        auto& pillar = engine.createObject(name, vkengine::MeshType::Cube);
        pillar.transform().position = glm::vec3{-8.0f + i * 4.0f, 2.0f, 8.0f};
        pillar.transform().scale = glm::vec3{0.5f, 4.0f, 0.5f};
        pillar.setMaterial("stone");
    }
    
    // Add a large central cube
    auto& hero = engine.createObject("hero_cube", vkengine::MeshType::Cube);
    hero.transform().position = glm::vec3{0.0f, 1.5f, -3.0f};
    hero.transform().scale = glm::vec3{1.5f, 1.5f, 1.5f};
    hero.transform().rotation = glm::vec3{0.0f, 45.0f, 0.0f};
    hero.setMaterial("gold");
    
    // Set up lights
    vkengine::LightCreateInfo sunLight{};
    sunLight.name = "SunLight";
    sunLight.position = glm::vec3{5.0f, 10.0f, 5.0f};
    sunLight.color = glm::vec3{1.0f, 0.95f, 0.85f};
    sunLight.intensity = 2.0f;
    scene.createLight(sunLight);
    
    // Accent point light
    vkengine::LightCreateInfo pointLight{};
    pointLight.name = "AccentLight";
    pointLight.position = glm::vec3{3.0f, 3.0f, 0.0f};
    pointLight.color = glm::vec3{0.4f, 0.6f, 1.0f};
    pointLight.intensity = 1.5f;
    scene.createLight(pointLight);
}

// Blur type names for UI
static const char* kBlurTypeNames[] = {
    "Gaussian", "Box", "Radial"
};

void renderUI(VulkanRenderer& renderer) {
    static int currentShaderStyle = 0;
    static VulkanRenderer::FogSettings fogSettings{};
    static VulkanRenderer::BlurSettings blurSettings{};
    static bool initialized = false;
    
    if (!initialized) {
        fogSettings = renderer.getFogSettings();
        blurSettings = renderer.getBlurSettings();
        currentShaderStyle = static_cast<int>(renderer.getShaderStyle());
        initialized = true;
    }
    
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 500), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Visual Effects Controls")) {
        // Presets
        ImGui::Text("Quick Presets:");
        ImGui::Separator();
        
        for (size_t i = 0; i < kPresets.size(); ++i) {
            if (i > 0 && i % 3 != 0) ImGui::SameLine();
            if (ImGui::Button(kPresets[i].name, ImVec2(95, 25))) {
                renderer.setShaderStyle(kPresets[i].shaderStyle);
                renderer.setFogSettings(kPresets[i].fog);
                currentShaderStyle = static_cast<int>(kPresets[i].shaderStyle);
                fogSettings = kPresets[i].fog;
            }
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Shader Style
        if (ImGui::CollapsingHeader("Shader Style", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Combo("Style", &currentShaderStyle, kShaderStyleNames, 7)) {
                renderer.setShaderStyle(static_cast<VulkanRenderer::ShaderStyle>(currentShaderStyle));
            }
            
            ImGui::TextWrapped("Try different shader styles to see how they affect the scene appearance.");
        }
        
        // Fog Settings
        if (ImGui::CollapsingHeader("Fog", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool fogChanged = false;
            
            fogChanged |= ImGui::Checkbox("Enable Fog", &fogSettings.enabled);
            
            if (fogSettings.enabled) {
                fogChanged |= ImGui::ColorEdit3("Fog Color", &fogSettings.color.x);
                fogChanged |= ImGui::SliderFloat("Density", &fogSettings.density, 0.001f, 0.2f, "%.3f");
                fogChanged |= ImGui::SliderFloat("Height Falloff", &fogSettings.heightFalloff, 0.0f, 1.0f);
                fogChanged |= ImGui::SliderFloat("Height Offset", &fogSettings.heightOffset, -10.0f, 10.0f);
                fogChanged |= ImGui::SliderFloat("Start Distance", &fogSettings.startDistance, 0.0f, 20.0f);
                
                ImGui::TextWrapped("Adjust fog parameters to simulate atmospheric effects like mist or haze.");
            }
            
            if (fogChanged) {
                renderer.setFogSettings(fogSettings);
            }
        }
        
        // Blur Settings
        if (ImGui::CollapsingHeader("Blur", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool blurChanged = false;
            
            blurChanged |= ImGui::Checkbox("Enable Blur", &blurSettings.enabled);
            
            if (blurSettings.enabled) {
                int blurType = static_cast<int>(blurSettings.type);
                if (ImGui::Combo("Blur Type", &blurType, kBlurTypeNames, 3)) {
                    blurSettings.type = static_cast<VulkanRenderer::BlurType>(blurType);
                    blurChanged = true;
                }
                
                blurChanged |= ImGui::SliderFloat("Radius", &blurSettings.radius, 1.0f, 10.0f, "%.1f");
                
                if (blurSettings.type == VulkanRenderer::BlurType::Gaussian) {
                    blurChanged |= ImGui::SliderFloat("Sigma", &blurSettings.sigma, 0.5f, 10.0f, "%.1f");
                    ImGui::TextWrapped("Gaussian blur creates a smooth, natural-looking blur.");
                } else if (blurSettings.type == VulkanRenderer::BlurType::Box) {
                    ImGui::TextWrapped("Box blur is faster but creates a more uniform blur.");
                } else if (blurSettings.type == VulkanRenderer::BlurType::Radial) {
                    blurChanged |= ImGui::SliderInt("Samples", &blurSettings.sampleCount, 4, 32);
                    blurChanged |= ImGui::SliderFloat2("Center", &blurSettings.center.x, 0.0f, 1.0f);
                    ImGui::TextWrapped("Radial blur creates a zoom/motion effect from the center point.");
                }
            }
            
            if (blurChanged) {
                renderer.setBlurSettings(blurSettings);
            }
            
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), 
                "Blur is active! Adjust the parameters to see the effect.");
        }
        
        // Info section
        if (ImGui::CollapsingHeader("Info")) {
            ImGui::TextWrapped(
                "This demo showcases the visual effects currently "
                "available in the EVE Engine renderer:\n\n"
                "• Shader Styles: Change how objects are lit and shaded\n"
                "• Fog: Add atmospheric depth with height-based fog\n"
                "• Shadows: Dynamic shadow mapping (always on)\n\n"
                "Use the controls above or click a preset to see different effects."
            );
        }
        
        // Keyboard shortcuts
        if (ImGui::CollapsingHeader("Keyboard Shortcuts")) {
            ImGui::BulletText("V: Cycle visualization modes");
            ImGui::BulletText("G: Cycle shader styles");
            ImGui::BulletText("WASD: Move camera");
            ImGui::BulletText("Mouse: Look around");
            ImGui::BulletText("ESC: Exit");
        }
    }
    ImGui::End();
}

int main(int argc, char** argv) {
    try {
        // Create engine and scene
        vkengine::GameEngine engine{};
        setupScene(engine);
        
        // Create renderer
        VulkanRenderer renderer{engine};
        
        // Configure window
        WindowConfig windowConfig{};
        windowConfig.title = "EVE Engine - Visual Effects Demo";
        windowConfig.width = 1280;
        windowConfig.height = 720;
        windowConfig.resizable = true;
        renderer.setWindowConfig(windowConfig);
        
        // Enable default fog to show something interesting
        VulkanRenderer::FogSettings defaultFog{};
        defaultFog.enabled = true;
        defaultFog.color = glm::vec3{0.7f, 0.75f, 0.8f};
        defaultFog.density = 0.04f;
        defaultFog.heightFalloff = 0.15f;
        defaultFog.heightOffset = 0.0f;
        defaultFog.startDistance = 5.0f;
        renderer.setFogSettings(defaultFog);
        
        // Check for headless capture
        const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "PostProcessingExample");
        if (capture.enabled) {
            WindowConfig headlessWindow{};
            headlessWindow.width = capture.width;
            headlessWindow.height = capture.height;
            headlessWindow.headless = true;
            headlessWindow.resizable = false;
            renderer.setWindowConfig(headlessWindow);
            renderer.setSceneControlsEnabled(false);
            renderer.setLightAnimationEnabled(false);
            std::filesystem::create_directories(capture.outputPath.parent_path());
            return renderer.renderSingleFrameToJpeg(capture.outputPath) ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        
        // Set up UI callback
        renderer.setCustomUiCallback([&renderer](float /*deltaTime*/) {
            renderUI(renderer);
        });
        
        std::cout << "=== EVE Engine Visual Effects Demo ===\n";
        std::cout << "Controls:\n";
        std::cout << "  WASD      - Move camera\n";
        std::cout << "  Mouse     - Look around\n";
        std::cout << "  V         - Cycle visualization modes\n";
        std::cout << "  G         - Cycle shader styles\n";
        std::cout << "  ESC       - Exit\n";
        std::cout << "\nUse the ImGui panel to adjust visual effects!\n";
        
        // Run the renderer
        renderer.run();
        
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
