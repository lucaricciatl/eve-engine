#include "engine/GameEngine.hpp"
#include "engine/HeadlessCapture.hpp"
#include "VulkanRenderer.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

WindowMode parseMode(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        std::string arg{argv[i]};
        if (arg == "--fullscreen") {
            return WindowMode::Fullscreen;
        }
        if (arg == "--borderless") {
            return WindowMode::Borderless;
        }
        if (arg == "--fixed") {
            return WindowMode::FixedSize;
        }
    }
    return WindowMode::Windowed;
}

bool parseFlag(int argc, char** argv, const char* flag)
{
    for (int i = 1; i < argc; ++i) {
        if (std::string{argv[i]} == flag) {
            return true;
        }
    }
    return false;
}

float parseOpacity(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        std::string arg{argv[i]};
        const std::string prefix = "--opacity=";
        if (arg.rfind(prefix, 0) == 0) {
            try {
                const float value = std::stof(arg.substr(prefix.size()));
                return std::clamp(value, 0.0f, 1.0f);
            } catch (...) {
                return 0.6f;
            }
        }
    }
    return 0.6f; // default semi-transparent so desktop shows through
}

void printHelp()
{
    std::cout << "Window Features Demo\n"
              << "  F5-F8 : switch modes (windowed/borderless/fixed/fullscreen)\n"
              << "  F9    : toggle opacity between 0.6 and 1.0\n"
              << "  Esc   : exit\n"
              << "  CLI   : --opacity=<0..1>\n";
}

void buildScene(vkengine::GameEngine& engine)
{
    auto& scene = engine.scene();
    scene.clear();

    auto& camera = scene.camera();
    camera.setPosition({3.0f, 2.0f, 6.0f});
    camera.setYawPitch(glm::radians(-120.0f), glm::radians(-15.0f));
    camera.setMovementSpeed(4.0f);

    auto& ground = engine.createObject("Ground", vkengine::MeshType::Cube);
    ground.transform().scale = {6.0f, 0.25f, 6.0f};
    ground.transform().position = {0.0f, -1.0f, 0.0f};
    ground.enableCollider({3.0f, 0.125f, 3.0f}, true);
    ground.setBaseColor({0.35f, 0.35f, 0.4f});

    auto& box = engine.createObject("Box", vkengine::MeshType::Cube);
    box.transform().scale = {0.8f, 0.8f, 0.8f};
    box.transform().position = {0.0f, 0.3f, 0.0f};
    box.enableCollider({0.4f, 0.4f, 0.4f});
    box.setBaseColor({0.9f, 0.4f, 0.3f});

    vkengine::LightCreateInfo key{};
    key.name = "Key";
    key.position = {3.0f, 4.0f, 2.0f};
    key.color = {1.0f, 0.95f, 0.85f};
    key.intensity = 6.0f;
    scene.createLight(key);
}

} // namespace

int main(int argc, char** argv)
try {
    vkengine::GameEngine engine;
    WindowConfig config{};
    config.title = "Window Features Demo";
    config.mode = parseMode(argc, argv);
    config.transparentFramebuffer = true; // allow seeing through the window
    const bool fixedSize = (config.mode == WindowMode::FixedSize);
    config.resizable = !fixedSize && !parseFlag(argc, argv, "--no-resize");
    config.opacity = parseOpacity(argc, argv);

    // Keep the scene empty to render only a black background.
    engine.scene().clear();

    std::cout << "Starting window with mode=";
    switch (config.mode) {
    case WindowMode::Windowed: std::cout << "Windowed"; break;
    case WindowMode::Borderless: std::cout << "Borderless"; break;
    case WindowMode::Fullscreen: std::cout << "Fullscreen"; break;
    case WindowMode::FixedSize: std::cout << "FixedSize"; break;
    }
    std::cout << ", transparent=" << (config.transparentFramebuffer ? "true" : "false")
              << ", resizable=" << (config.resizable ? "true" : "false")
              << ", opacity=" << config.opacity << '\n';

    VulkanRenderer renderer(engine);
    renderer.setWindowConfig(config);
    renderer.setLightAnimationEnabled(false);
    renderer.setWindowOpacity(config.opacity);
    printHelp();

    renderer.setCustomUiCallback([&renderer](float /*deltaSeconds*/) {
    static bool prevF5 = false;
    static bool prevF6 = false;
    static bool prevF7 = false;
    static bool prevF8 = false;
    static bool prevF9 = false;

        GLFWwindow* wnd = renderer.getWindow();
        if (!wnd) {
            return;
        }

        auto handleKey = [&](int key, bool& prev, WindowMode mode) {
            const bool pressed = glfwGetKey(wnd, key) == GLFW_PRESS;
            if (pressed && !prev) {
                renderer.setWindowMode(mode);
            }
            prev = pressed;
        };

        handleKey(GLFW_KEY_F5, prevF5, WindowMode::Windowed);
        handleKey(GLFW_KEY_F6, prevF6, WindowMode::Borderless);
        handleKey(GLFW_KEY_F7, prevF7, WindowMode::FixedSize);
        handleKey(GLFW_KEY_F8, prevF8, WindowMode::Fullscreen);

        // F9: toggle between default opacity and full opaque
        const bool pressedF9 = glfwGetKey(wnd, GLFW_KEY_F9) == GLFW_PRESS;
        if (pressedF9 && !prevF9) {
            static bool dimmed = true;
            dimmed = !dimmed;
            renderer.setWindowOpacity(dimmed ? 0.6f : 1.0f);
        }
        prevF9 = pressedF9;

        // Draw opaque rectangle in the center
        ImVec2 display = ImGui::GetIO().DisplaySize;
        const float rectW = 240.0f;
        const float rectH = 140.0f;
        ImVec2 center{display.x * 0.5f, display.y * 0.5f};
        ImVec2 p0{center.x - rectW * 0.5f, center.y - rectH * 0.5f};
        ImVec2 p1{center.x + rectW * 0.5f, center.y + rectH * 0.5f};
        ImGui::GetBackgroundDrawList()->AddRectFilled(p0, p1, IM_COL32(30, 144, 255, 255)); // opaque
    });

    const auto capture = vkengine::parseHeadlessCaptureArgs(argc, argv, "WindowFeaturesExample");
    if (capture.enabled) {
        WindowConfig headlessConfig = config;
        headlessConfig.width = capture.width;
        headlessConfig.height = capture.height;
        headlessConfig.headless = true;
        headlessConfig.resizable = false;
        renderer.setWindowConfig(headlessConfig);
        renderer.setSceneControlsEnabled(false);
        std::filesystem::create_directories(capture.outputPath.parent_path());
        return renderer.renderSingleFrameToJpeg(capture.outputPath) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    renderer.run();

    return 0;
} catch (const std::exception& e) {
    std::cerr << "Window features example failed: " << e.what() << '\n';
    return 1;
}
