#include "engine/GameEngine.hpp"
#include "engine/HeadlessCapture.hpp"
#include "VulkanRenderer.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <imgui.h>

namespace {
std::filesystem::path resolveAsset(const std::filesystem::path& relative)
{
    // Start from the executable directory and walk up a few parents to find the asset
    std::error_code ec;
    auto exeDir = std::filesystem::current_path(ec);
    if (ec) {
        exeDir = std::filesystem::path{};
    }

    for (int i = 0; i < 4; ++i) {
        const auto candidate = std::filesystem::weakly_canonical(exeDir / relative, ec);
        if (!ec && std::filesystem::exists(candidate)) {
            return candidate;
        }
        if (exeDir.has_parent_path()) {
            exeDir = exeDir.parent_path();
        }
    }
    return relative; // Fall back to relative; renderer will try its own resolver
}
} // namespace

int main(int argc, char** argv)
try {
    vkengine::GameEngine engine;
    engine.scene().clear();

    VulkanRenderer renderer(engine);

    const std::filesystem::path skyPath = resolveAsset("assets/skys/sky.jpg");

    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "SkyImageBackgroundExample");
    if (capture.enabled) {
        WindowConfig window{};
        window.width = capture.width;
        window.height = capture.height;
        window.headless = true;
        window.resizable = false;
        renderer.setWindowConfig(window);
        renderer.setSceneControlsEnabled(false);
        renderer.setLightAnimationEnabled(false);
        renderer.setSkyColor({0.05f, 0.08f, 0.15f});
        std::filesystem::create_directories(capture.outputPath.parent_path());
        return renderer.renderSingleFrameToJpeg(capture.outputPath) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (renderer.setSkyFromFile(skyPath)) {
        std::cout << "Loaded sky from " << skyPath.string() << " and set average color as background\n";
    } else {
        std::cout << "Failed to load sky image; falling back to default color\n";
        renderer.setSkyColor({0.05f, 0.08f, 0.15f});
    }

    // Lightweight overlay to verify the sky image is active
    renderer.setCustomUiCallback([skyPath](float) {
        ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.35f);
        if (ImGui::Begin("Sky Preview")) {
            ImGui::TextWrapped("Sky texture: %s", skyPath.string().c_str());
            ImGui::TextWrapped("The loaded sky image is drawn behind the scene as a full-screen background.");
        }
        ImGui::End();
    });

    renderer.run();
    return 0;
} catch (const std::exception& e) {
    std::cerr << "Sky image background demo failed: " << e.what() << '\n';
    return 1;
}
