#include "core/VulkanRenderer.hpp"
#include "engine/HeadlessCapture.hpp"
#include "engine/GameEngine.hpp"
#include "ui/Draw.hpp"
#include "ui/CustomUi.hpp"

#include <glm/glm.hpp>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <vector>

int main(int argc, char** argv)
try {
    vkengine::GameEngine engine;
    auto& scene = engine.scene();
    scene.clear();

    // Camera
    auto& camera = scene.camera();
    // Initial camera position; it will orbit in the callback below.
    camera.setPosition(glm::vec3{0.0f, 0.0f, 6.0f});
    camera.lookAt(glm::vec3{0.0f, 0.0f, 0.0f});

    // Light (optional: minimal fill)
    vkengine::LightCreateInfo lightInfo{};
    lightInfo.name = "Key";
    lightInfo.position = glm::vec3{5.0f, 6.0f, 5.0f};
    lightInfo.color = glm::vec3{1.0f, 0.95f, 0.85f};
    lightInfo.intensity = 1.0f;
    scene.createLight(lightInfo);

    VulkanCubeApp renderer(engine);
    renderer.setLightAnimationEnabled(false);

    renderer.setCustomUiCallback([&engine](float dt) {
        // Draw directly to the foreground draw list—no ImGui windows.
        if (ImGui::GetCurrentContext() == nullptr) {
            return;
        }
        ImGuiIO& io = ImGui::GetIO();
        if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) {
            return;
        }

    static float t = 0.0f;
    t += io.DeltaTime;

    static float orbitSpeed = 0.6f;
    static bool orbitReverse = false;
    static float sizeScale = 1.0f;

    // Simple orbital camera: rotate around origin at radius 6
    auto& cam = engine.scene().camera();
    float orbitRadius = 6.0f;
    float yaw = (orbitReverse ? -orbitSpeed : orbitSpeed) * t;
    cam.setPosition(glm::vec3{orbitRadius * std::cos(yaw), 1.2f, orbitRadius * std::sin(yaw)});
    cam.lookAt(glm::vec3{0.0f, 0.0f, 0.0f});

        ImVec2 center{io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f};
        float radius = 40.0f + sizeScale * 60.0f;

        std::vector<ImVec2> tri{
            {center.x + radius * std::cos(t), center.y + radius * std::sin(t)},
            {center.x + radius * std::cos(t + 2.094f), center.y + radius * std::sin(t + 2.094f)},
            {center.x + radius * std::cos(t + 4.188f), center.y + radius * std::sin(t + 4.188f)}
        };

        vkui::DrawPolygonFilled(tri, {0.3f, 0.7f, 1.0f, 0.35f}, {true});
        vkui::DrawPolyline(tri, {0.1f, 0.6f, 1.0f, 1.0f}, true, 2.0f, {true});

        vkui::DrawLine(ImVec2{center.x - 80.0f, center.y}, ImVec2{center.x + 80.0f, center.y}, {1.0f, 0.4f, 0.4f, 1.0f}, 2.0f, {true});
        vkui::DrawLine(ImVec2{center.x, center.y - 80.0f}, ImVec2{center.x, center.y + 80.0f}, {0.4f, 1.0f, 0.4f, 1.0f}, 2.0f, {true});

        ImVec2 rectMin{center.x - 110.0f, center.y - 70.0f};
        ImVec2 rectMax{center.x - 40.0f, center.y - 20.0f};
        vkui::DrawRectFilled(rectMin, rectMax, {0.9f, 0.8f, 0.2f, 0.3f}, 6.0f, {true});
        vkui::DrawRect(rectMin, rectMax, {0.9f, 0.8f, 0.2f, 1.0f}, 6.0f, 2.0f, {true});

        // Draw a small point marker at center
        ImVec2 ptMin{center.x - 3.0f, center.y - 3.0f};
        ImVec2 ptMax{center.x + 3.0f, center.y + 3.0f};
        vkui::DrawRectFilled(ptMin, ptMax, {1.0f, 1.0f, 1.0f, 1.0f}, 3.0f, {true});

        // Custom UI window (drawn via Vulkan primitives, no ImGui windows)
        auto win = vkui::BeginWindow(ImVec2{20.0f, 20.0f}, ImVec2{260.0f, 190.0f}, "Custom Vulkan UI");
        vkui::Label(win.content, "Controls");
        if (vkui::Button(win.content, orbitReverse ? "Orbit: reverse" : "Orbit: forward")) {
            orbitReverse = !orbitReverse;
        }
        vkui::SliderFloat(win.content, "Orbit speed", &orbitSpeed, 0.1f, 1.5f);
        vkui::SliderFloat(win.content, "Shape size", &sizeScale, 0.3f, 1.6f);
        vkui::EndWindow(win);

        // Custom HUD text near the shapes
        char hud[128];
        std::snprintf(hud, sizeof(hud), "Custom UI | Orbit t=%.2f", t);
        vkui::DrawText(ImVec2{center.x - 100.0f, center.y - 110.0f}, hud, {1.0f, 1.0f, 1.0f, 1.0f}, {true});
    });

    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "DrawOverlayExample");
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
    fprintf(stderr, "Draw overlay demo failed: %s\n", e.what());
    return EXIT_FAILURE;
}
