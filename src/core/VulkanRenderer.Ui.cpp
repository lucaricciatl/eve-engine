#include "core/VulkanRenderer.hpp"

#include <algorithm>
#include <iostream>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

void VulkanRenderer::initUi()
{
    if (!uiEnabled || uiLayer.isInitialized()) {
        return;
    }

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uiLayer.initialize(window, instance, physicalDevice, device,
                       indices.graphicsFamily.value(), graphicsQueue,
                       renderPass, static_cast<uint32_t>(swapChainImages.size()),
                       static_cast<uint32_t>(swapChainImages.size()));

    VkCommandBuffer cmd = beginSingleTimeCommands();
    uiLayer.uploadFonts(cmd);
    endSingleTimeCommands(cmd);
}

void VulkanRenderer::destroyUi()
{
    uiLayer.shutdown(device);
}

void VulkanRenderer::rebuildUi()
{
    if (!uiLayer.isInitialized()) {
        return;
    }
    uiLayer.onSwapchainRecreated(renderPass,
                                 static_cast<uint32_t>(swapChainImages.size()),
                                 static_cast<uint32_t>(swapChainImages.size()));
    VkCommandBuffer cmd = beginSingleTimeCommands();
    uiLayer.uploadFonts(cmd);
    endSingleTimeCommands(cmd);
}

void VulkanRenderer::buildUi(float deltaSeconds)
{
    if (!uiEnabled || !uiLayer.isInitialized()) {
        return;
    }

    if (engine == nullptr) {
        return;
    }

    ensureSkyTextureLoaded();
    if (skyTextureLoaded && skyTextureId != VK_NULL_HANDLE) {
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        const ImVec2 topLeft{0.0f, 0.0f};
        const ImVec2 bottomRight{static_cast<float>(swapChainExtent.width),
                                static_cast<float>(swapChainExtent.height)};
        bg->AddImage(reinterpret_cast<ImTextureID>(skyTextureId), topLeft, bottomRight);
    }

    ImGui::GetIO().DeltaTime = std::max(0.0001f, deltaSeconds);

    const float fps = (deltaSeconds > 0.0f) ? (1.0f / deltaSeconds) : 0.0f;
    smoothedFps = smoothedFps * 0.9f + fps * 0.1f;

    if (sceneControlsEnabled) {
        ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Scene Controls")) {
            ImGui::Text("FPS: %.1f", smoothedFps);
            ImGui::Checkbox("Enable shadows", &shadowsEnabled);
            ImGui::Checkbox("Enable specular", &specularEnabled);
            ImGui::Checkbox("Animate key light", &animateLight);
            ImGui::Checkbox("Show ImGui demo", &showUiDemo);

            static const char* kStyleLabels[] = {
                "Standard",
                "Toon",
                "Rim",
                "Heat",
                "Gradient",
                "Wireframe",
                "Glow"
            };
            int styleIndex = static_cast<int>(shaderStyle);
            if (ImGui::Combo("Shader style", &styleIndex, kStyleLabels, static_cast<int>(IM_ARRAYSIZE(kStyleLabels)))) {
                shaderStyle = static_cast<ShaderStyle>(styleIndex);
            }

            auto& camera = engine->scene().camera();
            float moveSpeed = camera.getMovementSpeed();
            if (ImGui::SliderFloat("Camera speed", &moveSpeed, 0.5f, 20.0f)) {
                camera.setMovementSpeed(moveSpeed);
            }

            ImGui::Separator();
            auto& physics = engine->physics();
            glm::vec3 gravity = physics.getGravity();
            if (ImGui::SliderFloat3("Gravity", &gravity.x, -25.0f, 0.0f, "%.2f")) {
                physics.setGravity(gravity);
            }

            if (!engine->scene().lights().empty()) {
                auto& light = engine->scene().lights().front();
                glm::vec3 color = light.color();
                if (ImGui::ColorEdit3("Light color", &color.x)) {
                    light.setColor(color);
                }
                float intensity = light.intensity();
                if (ImGui::SliderFloat("Light intensity", &intensity, 0.1f, 20.0f, "%.2f")) {
                    light.setIntensity(intensity);
                }
            }

            ImGui::Separator();
            ImGui::Text("Volumetric Fog");
            ImGui::Checkbox("Enable fog", &fogSettings.enabled);
            if (fogSettings.enabled) {
                ImGui::ColorEdit3("Fog color", &fogSettings.color.x);
                ImGui::SliderFloat("Fog density", &fogSettings.density, 0.0f, 0.2f, "%.3f");
                ImGui::SliderFloat("Fog height falloff", &fogSettings.heightFalloff, 0.0f, 1.0f, "%.3f");
                ImGui::SliderFloat("Fog height offset", &fogSettings.heightOffset, -5.0f, 5.0f, "%.2f");
                ImGui::SliderFloat("Fog start distance", &fogSettings.startDistance, 0.0f, 10.0f, "%.2f");
            }
        }
        ImGui::End();
    }

    if (showUiDemo) {
        ImGui::ShowDemoWindow(&showUiDemo);
    }

    if (console.enabled) {
        ImGui::SetNextWindowSize(ImVec2(520.0f, 260.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Console", &console.enabled)) {
            ImGui::Checkbox("Auto-scroll", &console.autoScroll);
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                console.lines.clear();
            }

            ImGui::Separator();
            ImGui::BeginChild("ConsoleScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 6.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
            for (const auto& line : console.lines) {
                ImGui::TextUnformatted(line.c_str());
            }
            if (console.autoScroll && console.scrollToBottom) {
                ImGui::SetScrollHereY(1.0f);
            }
            console.scrollToBottom = false;
            ImGui::EndChild();

            ImGui::Separator();
            const std::string label = console.prompt + ">";
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText(label.c_str(), &console.input, ImGuiInputTextFlags_EnterReturnsTrue)) {
                const std::string line = console.input;
                console.input.clear();
                if (!line.empty()) {
                    addConsoleLine(console.prompt + "> " + line);
                }
            }
        }
        ImGui::End();
    }

    if (customUi) {
        customUi(deltaSeconds);
    }
}

void VulkanRenderer::addConsoleLine(std::string line)
{
    if (line.empty()) {
        return;
    }
    console.lines.emplace_back(std::move(line));
    if (static_cast<int>(console.lines.size()) > console.maxLines) {
        const auto overflow = console.lines.size() - static_cast<size_t>(console.maxLines);
        console.lines.erase(console.lines.begin(), console.lines.begin() + static_cast<std::ptrdiff_t>(overflow));
    }
    console.scrollToBottom = true;
}

void VulkanRenderer::framebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/)
{
    auto app = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}

void VulkanRenderer::setupInputCallbacks()
{
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
}

void VulkanRenderer::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
{
    auto* app = static_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
    if (app == nullptr) {
        return;
    }
    app->inputManager.handleMousePosition(xpos, ypos);
    if (app->customCursorCallback) {
        app->customCursorCallback(xpos, ypos);
    }
}

void VulkanRenderer::mouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/)
{
    auto* app = static_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
    if (app == nullptr || button != GLFW_MOUSE_BUTTON_RIGHT) {
        if (app != nullptr && app->customMouseButtonCallback) {
            double cursorX = 0.0;
            double cursorY = 0.0;
            glfwGetCursorPos(window, &cursorX, &cursorY);
            app->customMouseButtonCallback(button, action, cursorX, cursorY);
        }
        return;
    }

    if (action == GLFW_PRESS) {
        app->inputManager.enableMouseLook(true);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else if (action == GLFW_RELEASE) {
        app->inputManager.enableMouseLook(false);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    if (app->customMouseButtonCallback) {
        double cursorX = 0.0;
        double cursorY = 0.0;
        glfwGetCursorPos(window, &cursorX, &cursorY);
        app->customMouseButtonCallback(button, action, cursorX, cursorY);
    }
}

void VulkanRenderer::handleHotkeys(int key, int action)
{
    if (action != GLFW_PRESS) {
        return;
    }

    switch (key) {
    case GLFW_KEY_F1:
        cycleVisualizationMode();
        break;
    case GLFW_KEY_F2:
        shadowsEnabled = !shadowsEnabled;
        std::cout << "Shadows " << (shadowsEnabled ? "enabled" : "disabled") << '\n';
        break;
    case GLFW_KEY_F3:
        specularEnabled = !specularEnabled;
        std::cout << "Specular highlights " << (specularEnabled ? "enabled" : "disabled") << '\n';
        break;
    case GLFW_KEY_F4:
        animateLight = !animateLight;
        std::cout << "Light animation " << (animateLight ? "enabled" : "disabled") << '\n';
        break;
    case GLFW_KEY_F5:
        cycleShaderStyle();
        break;
    default:
        break;
    }
}

void VulkanRenderer::cycleVisualizationMode()
{
    static constexpr const char* kModeLabels[] = {
        "Final lighting",
        "Albedo only",
        "Normals",
        "Shadow factor",
        "Fog factor"
    };
    constexpr uint32_t modeCount = static_cast<uint32_t>(sizeof(kModeLabels) / sizeof(kModeLabels[0]));

    const uint32_t nextIndex = (static_cast<uint32_t>(visualizationMode) + 1u) % modeCount;
    visualizationMode = static_cast<VisualizationMode>(nextIndex);

    std::cout << "Shading view: " << kModeLabels[nextIndex] << '\n';
}

void VulkanRenderer::cycleShaderStyle()
{
    static constexpr const char* kStyleLabels[] = {
        "Standard",
        "Toon",
        "Rim",
        "Heat",
        "Gradient",
        "Wireframe",
        "Glow"
    };
    constexpr uint32_t styleCount = static_cast<uint32_t>(sizeof(kStyleLabels) / sizeof(kStyleLabels[0]));

    const uint32_t nextIndex = (static_cast<uint32_t>(shaderStyle) + 1u) % styleCount;
    shaderStyle = static_cast<ShaderStyle>(nextIndex);

    std::cout << "Shader style: " << kStyleLabels[nextIndex] << '\n';
}

void VulkanRenderer::printHotkeyHelp() const
{
    std::cout << "Hotkeys - F1: cycle shading views, F2: toggle shadows, F3: toggle specular, F4: toggle light animation, F5: cycle shader style" << '\n';
}

void VulkanRenderer::setCustomCursorCallback(CursorCallback callback)
{
    customCursorCallback = std::move(callback);
}

void VulkanRenderer::setCustomMouseButtonCallback(MouseButtonCallback callback)
{
    customMouseButtonCallback = std::move(callback);
}

void VulkanRenderer::keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/)
{
    auto* app = static_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
    if (app == nullptr) {
        return;
    }

    const bool pressed = action == GLFW_PRESS || action == GLFW_REPEAT;
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        app->inputManager.setKeyState(key, true);
    } else if (action == GLFW_RELEASE) {
        app->inputManager.setKeyState(key, false);
    }

    app->handleHotkeys(key, action);

    if (key == GLFW_KEY_ESCAPE && pressed && app->inputManager.isMouseLookActive()) {
        app->inputManager.enableMouseLook(false);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        return;
    }

    if (key == GLFW_KEY_ESCAPE && pressed && !app->inputManager.isMouseLookActive()) {
        app->requestExit();
    }
}
